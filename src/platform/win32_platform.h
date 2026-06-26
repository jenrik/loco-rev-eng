/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: WIN32_PLATFORM — window management, event loop, AND multiplayer
 *            networking layer (DirectPlay 4 / ENet)
 * PRIMARY PORTING TARGET: every Win32 API documented with Linux SDL2/POSIX equivalent
 *
 * This header is included by:
 *   win32_platform.c  — DirectPlay networking (DP_JoinSession, DP_ReceiveDispatch,
 *                        DP_SendMessage, Platform_DisplayError, NetObject_*, Thread_*)
 *   sdl_window.c      — Window creation and WndProc replacement
 *
 * Build with -DLOCO_LINUX to activate POSIX/SDL2/ENet code paths.
 */

#ifndef LOCO_WIN32_PLATFORM_H
#define LOCO_WIN32_PLATFORM_H

#include "../core/core.h"

/* =========================================================================
 * Platform detection and base includes
 * ========================================================================= */

#ifdef LOCO_LINUX
#  include <stdint.h>
#  include <stddef.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <pthread.h>
#  include <SDL2/SDL.h>
#  include <enet/enet.h>
#else
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <dplay.h>
#  include <dplobby.h>
#  include <stdint.h>
#endif

/* =========================================================================
 * Platform-abstracted primitive types (networking subsystem)
 * ========================================================================= */

#ifdef LOCO_LINUX

/*
 * On Linux we do not have Win32 type definitions.
 * Provide thin equivalents so the networking struct definitions compile.
 */
#  ifndef _LOCO_WIN32_TYPES_DEFINED
#    define _LOCO_WIN32_TYPES_DEFINED
typedef uint32_t   DWORD;
typedef uint16_t   WORD;
typedef uint8_t    BYTE;
typedef int32_t    HRESULT;
typedef uint32_t   DPID;      /* DirectPlay player ID — opaque uint32 */
typedef void      *LPVOID;
typedef char      *LPSTR;
typedef uint16_t  *LPWSTR;
typedef uint32_t   UINT;
typedef int        BOOL;
#    ifndef TRUE
#      define TRUE  1
#      define FALSE 0
#    endif
typedef SDL_Window   *HWND;
typedef void         *HINSTANCE;
#  endif

/* Thread handle: pthread_t on Linux, HANDLE on Win32. */
typedef pthread_t    LOCO_THREAD_HANDLE;

/* Network interface: ENetHost* on Linux, IDirectPlay4A* on Win32. */
typedef ENetHost    *LOCO_NET_IFACE;
typedef ENetPeer    *LOCO_NET_PEER;

#else /* WIN32 */

typedef HANDLE          LOCO_THREAD_HANDLE;
typedef IDirectPlay4A  *LOCO_NET_IFACE;
typedef DPID            LOCO_NET_PEER;

#endif /* LOCO_LINUX */

/* =========================================================================
 * GUID type (platform-neutral, used for the Lego Loco application GUID)
 * ========================================================================= */

#ifndef _LOCO_GUID_DEFINED
#define _LOCO_GUID_DEFINED
typedef struct _LOCO_GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} LOCO_GUID;
#endif

/* =========================================================================
 * Win32 → SDL2/POSIX Quick-Reference  (window management subsystem)
 * All APIs found across the functions in this translation unit.
 * =========================================================================
 *
 * DISPLAY
 *   WIN32: GetDesktopWindow()
 *   LINUX: SDL_GetDisplayBounds(0, &rect)
 *
 *   WIN32: GetSystemMetrics(SM_CXSCREEN / SM_CYSCREEN)
 *   LINUX: SDL_GetCurrentDisplayMode(0, &mode)  → mode.w / mode.h
 *
 *   WIN32: GetDeviceCaps(hdc, BITSPIXEL=0x0c)
 *   LINUX: SDL_BITSPERPIXEL(mode.format)
 *
 *   WIN32: GetDeviceCaps(hdc, RASTERCAPS=0x18)  [palette / 8-bit check]
 *   LINUX: SDL_ISPIXELFORMAT_INDEXED(mode.format)
 *
 *   WIN32: GetSystemMetrics(SM_SAMEDISPLAYFORMAT=0x13)  [multi-monitor]
 *   LINUX: iterate SDL_GetNumDisplayModes() and compare pixel formats
 *
 *   WIN32: GetDC(hwnd) / ReleaseDC(hwnd, hdc)
 *   LINUX: not needed; SDL_GetCurrentDisplayMode gives the same info
 *
 * WINDOW
 *   WIN32: RegisterClassA(&wc)   [class "LEGO LOCO", CS_OWNDC|CS_DBLCLKS]
 *   LINUX: implicit in SDL_CreateWindow
 *
 *   WIN32: CreateWindowExA(WS_EX_TOPMOST, "LEGO LOCO", WS_POPUP|WS_VISIBLE,
 *                          0, 0, screenW, screenH, ...)
 *   LINUX: SDL_CreateWindow("LEGO LOCO", ...,
 *              SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN)
 *
 *   WIN32: GetClientRect(hwnd, &rect)
 *   LINUX: SDL_GetWindowSize(win, &w, &h)
 *
 *   WIN32: EnableWindow(hwnd, FALSE)   [block input during loading]
 *   LINUX: boolean "loading" flag; ignore SDL events in the poll loop
 *
 *   WIN32: InvalidateRect(hwnd, NULL, FALSE) + UpdateWindow(hwnd)
 *   LINUX: SDL_RenderPresent(renderer)
 *
 *   WIN32: SetCursor(NULL)    [hide cursor during loading]
 *   LINUX: SDL_ShowCursor(SDL_DISABLE)
 *
 *   WIN32: SetScrollRange(hwnd, SB_HORZ, 0, nMax, FALSE)
 *          SetScrollPos(hwnd, SB_HORZ, pos, TRUE)
 *   LINUX: track scroll offset as a plain integer; render custom sprite
 *
 *   WIN32: LoadIconA(hInst, MAKEINTRESOURCE(0x65))
 *   LINUX: SDL_LoadBMP("icon.bmp") + SDL_SetWindowIcon(win, surf)
 *
 *   WIN32: MessageBoxA(NULL, text, "LEGO LOCO", 0)
 *   LINUX: SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, win)
 *
 * MESSAGE LOOP
 *   WIN32: PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)
 *   LINUX: SDL_PeepEvents(&ev, 1, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)
 *
 *   WIN32: PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)
 *   LINUX: SDL_PollEvent(&ev)
 *
 *   WIN32: TranslateMessage(&msg)
 *   LINUX: implicit in SDL's keyboard event layer
 *
 *   WIN32: DispatchMessageA(&msg)
 *   LINUX: your SDL_PollEvent switch statement
 *
 *   WIN32: PostMessageA(hwnd, WM_QUIT=0x12, 0, 0)
 *   LINUX: SDL_Event ev = {.type=SDL_QUIT}; SDL_PushEvent(&ev)
 *
 *   WIN32: PostMessageA(hwnd, WM_APP+6=0x406, wParam, 0)
 *   LINUX: SDL_PushEvent with SDL_RegisterEvents() custom event type
 *
 *   WIN32: SendMessageA(hwnd, 0x407, flags, 0)   [synchronous flip request]
 *   LINUX: call SDL_RenderPresent(renderer) directly (synchronous)
 *
 *   WIN32: SetTimer(hwnd, 0x47, 150, NULL) / KillTimer(hwnd, id)
 *   LINUX: SDL_AddTimer(150, cb, NULL) / SDL_RemoveTimer(id)
 *
 *   WIN32: SetFocus(hwnd) / SetForegroundWindow(hwnd)
 *   LINUX: SDL_RaiseWindow(win)
 *
 *   WIN32: DefWindowProcA(hwnd, uMsg, wParam, lParam)
 *   LINUX: no equivalent; simply break / return default
 *
 * TIMER — 35 fps derivation
 *   WIN32: timeBeginPeriod(14)   [raise resolution to 14 ms — global]
 *   LINUX: no-op; Linux timers already resolve to ~1 ms
 *
 *   WIN32: timeSetEvent(28, 14, GameLoopCB, 0, TIME_PERIODIC)
 *   LINUX: SDL_AddTimer(28, TimerCB, NULL)
 *          In cb: push custom SDL_GAME_TICK event, return 28
 *          1000 / 28 ≈ 35.7 fps
 *
 *   WIN32: timeKillEvent(timerID)
 *   LINUX: SDL_RemoveTimer(timerID)
 *
 *   WIN32: timeEndPeriod(14)
 *   LINUX: no-op
 *
 * SYNC
 *   WIN32: CreateEventA(NULL, TRUE, FALSE, "GameLoop")
 *   LINUX: sem_init(&g_gameSem, 0, 0)  or  eventfd(0, EFD_SEMAPHORE)
 *
 *   WIN32: CloseHandle(hEvent)
 *   LINUX: sem_destroy(&g_gameSem)  or  close(fd)
 *
 *   WIN32: Sleep(100)   [busy-wait on worker thread]
 *   LINUX: pthread_join(workerThread, NULL)  or  sem_wait(&workerDone)
 *
 * REGISTRY
 *   WIN32: RegOpenKeyExA(HKLM, "SOFTWARE\\Intelligent Games\\LEGO LOCO", ...)
 *   LINUX: fopen(SDL_GetPrefPath("IntelligentGames", "LegoLoco"), "r")
 *
 *   WIN32: RegQueryValueExA(hKey, NULL, NULL, &type, buf, &len)
 *   LINUX: fread / inih / minIni parser
 *
 *   WIN32: RegCreateKeyExA(HKLM, subkey, ..., KEY_ALL_ACCESS, ..., &hKey, ...)
 *   LINUX: mkdir(prefPath) + fopen(path, "w")
 *
 *   WIN32: RegSetValueExA(hKey, NULL, 0, REG_SZ, data, len)
 *   LINUX: fputs(path, f)
 *
 *   WIN32: RegCloseKey(hKey)
 *   LINUX: fclose(f)
 *
 * AUDIO
 *   WIN32: PlaySoundA(NULL, NULL, 0)   [stop active .wav]
 *   LINUX: Mix_HaltChannel(-1)   (SDL_mixer)
 *
 * STRING / GEOMETRY
 *   WIN32: wsprintfA(buf, "Layouts\\%s", name)
 *   LINUX: snprintf(buf, sizeof buf, "Layouts/%s", name)
 *
 *   WIN32: SetRect(&r, l, t, w, h)
 *   LINUX: plain struct assignment  r = (SDL_Rect){l, t, w-l, h-t}
 *
 *   WIN32: DrawTextA(hdc, str, -1, &rc, DT_CALCRECT)
 *   LINUX: TTF_SizeText(font, str, &w, &h)
 *
 *   WIN32: SetTextColor(hdc, 0x2525dc)   [blue, selected item]
 *   LINUX: SDL_SetRenderDrawColor / SDL_ttf render with colour
 *
 * GDI / PAINT
 *   WIN32: BeginPaint(hwnd, &ps) / EndPaint(hwnd, &ps)
 *   LINUX: not needed; SDL_RenderPresent handles presentation
 *
 *   WIN32: SelectObject(hdc, hFont)
 *   LINUX: TTF_OpenFont / keep an SDL_ttf TTF_Font* per font face
 */

/* =========================================================================
 * Win32 message constants used by WndProc (0x4618c0)
 * Listed here so the C file compiles stand-alone under LOCO_LINUX.
 * ========================================================================= */
#ifndef WM_PAINT
#define WM_PAINT            0x000F
#endif
#ifndef WM_SIZE
#define WM_SIZE             0x0005
#endif
#ifndef WM_ACTIVATE
#define WM_ACTIVATE         0x0006
#endif
#ifndef WM_DESTROY
#define WM_DESTROY          0x0002
#endif
#ifndef WM_KEYDOWN
#define WM_KEYDOWN          0x0100
#endif
#ifndef WM_KEYUP
#define WM_KEYUP            0x0101
#endif
#ifndef WM_CHAR
#define WM_CHAR             0x0102
#endif
#ifndef WM_MOUSEMOVE
#define WM_MOUSEMOVE        0x0200
#endif
#ifndef WM_LBUTTONDOWN
#define WM_LBUTTONDOWN      0x0201
#endif
#ifndef WM_LBUTTONUP
#define WM_LBUTTONUP        0x0202
#endif
#ifndef WM_LBUTTONDBLCLK
#define WM_LBUTTONDBLCLK    0x0203
#endif
#ifndef WM_RBUTTONDOWN
#define WM_RBUTTONDOWN      0x0204
#endif
#ifndef WM_RBUTTONUP
#define WM_RBUTTONUP        0x0205
#endif
#ifndef WM_RBUTTONDBLCLK
#define WM_RBUTTONDBLCLK    0x0206
#endif
#ifndef WM_TIMER
#define WM_TIMER            0x0113
#endif
#ifndef WM_SYSCOMMAND
#define WM_SYSCOMMAND       0x0112
#endif
#ifndef WM_SETCURSOR
#define WM_SETCURSOR        0x0020
#endif
#ifndef WM_DISPLAYCHANGE
#define WM_DISPLAYCHANGE    0x007E
#endif

/* Custom private messages */
#define WM_LOCO_SURFACE_LOST  0x0405  /* WM_USER+5: DirectDraw surface recovery */
#define WM_LOCO_LOCALE_CHANGE 0x0406  /* WM_USER+6: locale/language switch       */
#define WM_LOCO_RENDER_FLIP   0x0407  /* WM_USER+7: DirectDraw flip / present    */
#define WM_LOCO_SUBSYS_CMD    0x0401  /* WM_USER+1: game subsystem commands       */

/* WM_SIZE wParam values */
#ifndef SIZE_RESTORED
#define SIZE_RESTORED   0
#define SIZE_MINIMIZED  1
#define SIZE_MAXIMIZED  2
#endif

/* WM_SYSCOMMAND wParam values used by WndProc */
#ifndef SC_MAXIMIZE
#define SC_MAXIMIZE     0xF030
#define SC_RESTORE      0xF120
#define SC_CLOSE        0xF060
#define SC_KEYMENU      0xF140
#endif

/* Virtual key codes referenced in WndProc */
#ifndef VK_RETURN
#define VK_RETURN       0x0D
#define VK_ESCAPE       0x1B
#endif

/* SetTimer ID used by FUN_00408350 / WM_TIMER handler */
#define LOCO_HEARTBEAT_TIMER_ID  0x47   /* 150 ms render-dirty heartbeat */

/* Scroll-bar direction constant */
#ifndef SB_HORZ
#define SB_HORZ         0
#endif

/* =========================================================================
 * Game state variables used by WndProc (cross-file globals from core.h)
 *
 * g_gameState (DAT_004851f4):
 *   0 = startup        1 = main menu     2 = loading
 *   3 = world/play     4 = dialog        8 = sub-dialog    10 = shutdown
 *
 * g_renderDirty (DAT_004aa4a4):
 *   Set to 1 by WM_TIMER(id=0x47) heartbeat; cleared after flip.
 *   LINUX: written by SDL_AddTimer callback; check in event loop.
 *
 * Mouse/keyboard state cache (written by WndProc, read by game logic):
 *   DAT_00485556 — mouse-moved pending flag
 *   DAT_004855ae — left-button held flag
 *   DAT_0048558c — left-button-up pending flag
 *   DAT_0048557c — right-button-down pending flag
 *   DAT_0048559c — right-button-up pending flag (uRam)
 *   DAT_00485558 — cached lParam from WM_MOUSEMOVE
 *   piRam00485570 — cached lParam from WM_LBUTTONDOWN
 *   piRam00485590 — cached lParam from WM_LBUTTONUP
 *   piRam00485580 — cached lParam from WM_RBUTTONDOWN
 *   piRam004855a0 — cached lParam from WM_RBUTTONUP
 *   DAT_004851f0  — minimised flag (0=normal, 1=iconified)
 * ========================================================================= */
extern volatile int  g_renderDirty;    /* DAT_004aa4a4 — render pending flag */

/* =========================================================================
 * ==========================================================================
 * SECTION 2: NETWORKING / DirectPlay 4 subsystem
 * ==========================================================================
 * Functions: DP_JoinSession (0x460360), DP_ReceiveDispatch (0x4606d0),
 *            DP_SendMessage (0x460d40), Platform_DisplayError (0x460ea0),
 *            NetObject_Construct (0x461610), NetObject_Destroy (0x461640),
 *            NetObject_ResetHandle (0x461690), Thread_SetPriorityIfRunning (0x4616c0)
 * ==========================================================================
 *
 * DirectPlay 4 → ENet replacement overview:
 *   IDirectPlay4A::Open      -> enet_host_connect()
 *   IDirectPlay4A::Receive   -> enet_host_service(host, &event, 0)
 *   IDirectPlay4A::Send      -> enet_peer_send()
 *   IDirectPlay4A::SendEx    -> enet_peer_send() with ENET_PACKET_FLAG_RELIABLE
 *   HeapAlloc/HeapFree       -> malloc / realloc / free
 *   LoadStringA              -> loco_load_string() (static string table)
 *   MessageBoxA              -> SDL_ShowSimpleMessageBox / fprintf(stderr)
 *   DialogBoxParamA          -> UDP broadcast discovery + SDL UI or CLI
 *   Sleep(1)                 -> SDL_Delay(1) or event-driven state machine
 *   WaitForSingleObject(h,0) -> pthread_kill(tid, 0) == 0
 *   SetThreadPriority        -> pthread_setschedparam + nice-value mapping
 *   CloseHandle (thread)     -> pthread_join
 *   GlobalFree               -> free()
 * ========================================================================= */

/* =========================================================================
 * Application / protocol constants (WIRE-FORMAT INVARIANTS)
 * These must be identical on every platform.
 * ========================================================================= */

/*
 * LOCO_DIRECTPLAY_GUID = {F9CD2546-577F-11D2-9426-00A0244BDA7A}
 *
 * Lego Loco's DirectPlay application GUID.  Used during session enumeration
 * so that only Lego Loco sessions are listed.  On Linux this must be embedded
 * as a magic handshake token in the ENet connection packet to maintain wire
 * compatibility with Windows peers still running DirectPlay.
 *
 * Stored little-endian in loco.exe at GameNetState+0x15a4 as four uint32s:
 *   0xf9cd2546, 0x11d2577f, 0xa0002694, 0x7ada4b24
 */
#define LOCO_DIRECTPLAY_GUID_DATA1   0xF9CD2546u
#define LOCO_DIRECTPLAY_GUID_DATA2   0x577Fu
#define LOCO_DIRECTPLAY_GUID_DATA3   0x11D2u
#define LOCO_DIRECTPLAY_GUID_DATA4_0 0x94u
#define LOCO_DIRECTPLAY_GUID_DATA4_1 0x26u
#define LOCO_DIRECTPLAY_GUID_DATA4_2 0x00u
#define LOCO_DIRECTPLAY_GUID_DATA4_3 0xA0u
#define LOCO_DIRECTPLAY_GUID_DATA4_4 0x24u
#define LOCO_DIRECTPLAY_GUID_DATA4_5 0x4Bu
#define LOCO_DIRECTPLAY_GUID_DATA4_6 0xDAu
#define LOCO_DIRECTPLAY_GUID_DATA4_7 0x7Au

/* Raw bytes of the GUID in wire byte order for handshake embedding. */
#define LOCO_DIRECTPLAY_GUID_BYTES \
    { 0x46, 0x25, 0xCD, 0xF9, \
      0x7F, 0x57,              \
      0xD2, 0x11,              \
      0x94, 0x26,              \
      0x00, 0xA0, 0x24, 0x4B, 0xDA, 0x7A }

/*
 * PROTO_VERSION = 300 (0x12c)
 * Lego Loco multiplayer protocol version number.
 * Must appear as a WORD at byte offset 2 of every network message.
 * Version mismatches trigger an error reply (type 0x1e) and session rejection.
 */
#define PROTO_VERSION               0x012Cu

/*
 * DPSESSIONDESC2_SIZE = 0x50 (80)
 * cbSize field value for the DPSESSIONDESC2 structure.
 * Matches the DirectX 6/7 SDK definition.
 */
#define DPSESSIONDESC2_SIZE         0x50u

/*
 * DPOPEN_FLAGS = 0x81
 * Flags for IDirectPlay4A::Open: DPOPEN_JOIN (0x01) | DPOPEN_RETURNSTATUS (0x80).
 * RETURNSTATUS causes an immediate return with DPERR_PENDING while connecting.
 */
#define DPOPEN_FLAGS                0x81u

/* =========================================================================
 * HRESULT / DirectPlay error codes
 * ========================================================================= */

/* DPERR_PENDING = MAKE_DPHRESULT(350) — async operation still in progress. */
#define DPERR_PENDING               ((HRESULT)0x8877015Eu)

/* DPERR_BUFFERTOOSMALL — buffer too small; grow and retry. */
#define LOCO_DPERR_BUFFERTOOSMALL   ((HRESULT)0x8877001Eu)

/* DPERR_NOMESSAGES — no messages pending; return NULL. */
#define LOCO_DPERR_NOMESSAGES       ((HRESULT)0x887700BEu)

/* DPWAITING — silently ignored in DP_SendMessage (async send pending). */
#define DPWAITING_SENTINEL          ((HRESULT)(-0x7ffffff6))

/* Fatal HRESULT that causes DP_JoinSession to return 0x88770100. */
#define DPERR_JOIN_FATAL            ((HRESULT)0x88770118u)
#define DPERR_JOIN_FATAL_RETURN     0x88770100u

/* =========================================================================
 * IDirectPlay4A vtable byte offsets
 * ========================================================================= */

#define IDP_OPEN_VTBL_OFFSET         0x34u   /* vtable index 13: Open */
#define IDP_CREATEPLAYER_VTBL_OFFSET 0x60u   /* vtable index 24: CreatePlayer / finalize */
#define IDP_SEND_VTBL_OFFSET         0x68u   /* vtable index 26: Send */
#define IDP_RECEIVE_VTBL_OFFSET      0x64u   /* vtable index 25: Receive (0x64 = 100 dec) */
#define IDP_SENDEX_VTBL_OFFSET       0xC4u   /* vtable index 49: SendEx */

/* =========================================================================
 * Send path flags
 * ========================================================================= */

/*
 * SEND_EX_FLAG = 0x10000
 * Bit in GameNetState+0x15e8.  When set, DP_SendMessage uses SendEx
 * instead of Send.  Linux: selects ENET_PACKET_FLAG_RELIABLE.
 */
#define SEND_EX_FLAG                0x10000u

/*
 * SENDEX_EXTRA_FLAGS = 0x600
 * OR'd into param_flags when using the SendEx path.
 * Encodes DPSEND_GUARANTEED (0x200) | DPSEND_OPENSTREAM (0x400).
 */
#define SENDEX_EXTRA_FLAGS          0x600u

/* =========================================================================
 * MessageBox / dialog flags
 * ========================================================================= */

/* MB_TOPMOST — ensures error dialog appears above fullscreen DirectDraw surfaces. */
#define LOCO_MB_TOPMOST             0x40000u

/* =========================================================================
 * Win32 thread primitives
 * ========================================================================= */

/* WAIT_TIMEOUT — WaitForSingleObject return: timeout expired, thread still running. */
#define LOCO_WAIT_TIMEOUT           0x102u

/* Win32 THREAD_PRIORITY_* constants (used in Thread_SetPriorityIfRunning). */
#ifndef THREAD_PRIORITY_IDLE
#define THREAD_PRIORITY_IDLE          (-15)
#define THREAD_PRIORITY_LOWEST        (-2)
#define THREAD_PRIORITY_BELOW_NORMAL  (-1)
#define THREAD_PRIORITY_NORMAL        (0)
#define THREAD_PRIORITY_ABOVE_NORMAL  (1)
#define THREAD_PRIORITY_HIGHEST       (2)
#define THREAD_PRIORITY_TIME_CRITICAL (15)
#endif

/* =========================================================================
 * String resource IDs
 * ========================================================================= */

#define LOCO_STR_BASE               0x7d00u  /* base of the Lego Loco string table */
#define STR_SEND_ERROR              0x7d03u  /* send failure error message */
#define STR_NET_ERROR_PREFIX        0x7d05u  /* session join error prefix */
#define STR_ERROR_TITLE             0x7d06u  /* dialog caption / MessageBox title */
#define DIALOG_SESSION_PICKER       0x7d0bu  /* session-picker dialog resource ID */

/* =========================================================================
 * Network protocol message type constants  (WIRE-FORMAT INVARIANTS)
 * ========================================================================= */

/* word at packet byte offset 0: */
#define MSG_TYPE_PLAYER_NAME    0x0003u  /* player name/data; max NAME_MAX_LEN bytes */
#define MSG_TYPE_STATUS         0x0005u  /* 8-byte status/ping */
#define MSG_TYPE_RESET          0x0031u  /* reset/disconnect; ACK with type 10 */
#define MSG_TYPE_SESSION_INFO   0x0032u  /* session accepted; terminates receive loop */
#define MSG_TYPE_CONNECT        0x0101u  /* player connected; ACK with type 0x3c */
#define MSG_TYPE_PLAYER_UPDATE  0x0103u  /* extended name/data (0x88-byte buffer) */
#define MSG_TYPE_NAME_CHECK     0x0104u  /* name collision detection */

/* ACK / reply message types: */
#define MSG_TYPE_ACK_RESET      0x000Au  /* reply for MSG_TYPE_RESET */
#define MSG_TYPE_ACK_CONNECT    0x003Cu  /* reply for MSG_TYPE_CONNECT */
#define MSG_TYPE_VERSION_ERR    0x001Eu  /* version mismatch reply */
#define MSG_TYPE_NAME_REPLY     0x005Au  /* name-check reply with remote name */

/* =========================================================================
 * Buffer / name constants  (WIRE-FORMAT INVARIANTS)
 * ========================================================================= */

/*
 * NAME_MAX_LEN = 128 (0x80)
 * Maximum player name length enforced by DP_ReceiveDispatch.
 * Bytes beyond 128 are truncated using save/restore of byte 129.
 */
#define NAME_MAX_LEN                128u

/*
 * RECEIVE_BUF_INIT = 0x7ff (2047)
 * Initial receive buffer size in DP_ReceiveDispatch.
 * Win32: HeapAlloc(heap, 0, 0x7ff); grows by 0x7ff on BUFFERTOOSMALL.
 * Linux: malloc(0x7ff); doubles on EAGAIN/EMSGSIZE.
 */
#define RECEIVE_BUF_INIT            0x7ffu

/* =========================================================================
 * Networking port (Linux extension — not in original binary)
 * ========================================================================= */

/*
 * LOCO_NET_PORT
 * UDP port for ENet sessions and discovery broadcasts on Linux.
 * Uses the historical DirectPlay default port (DirectPlay used this by default).
 */
#define LOCO_NET_PORT               47624u

/* =========================================================================
 * Struct definitions — networking subsystem
 * ========================================================================= */

/*
 * LOCO_DPSESSIONDESC2 — DirectPlay session descriptor.
 * Size: 0x50 (80) bytes.  Stored inline in GameNetState at offset +0x158c.
 *
 * On Linux this struct is kept for metadata serialisation; the actual
 * connection is managed by ENet.
 */
typedef struct _LOCO_DPSESSIONDESC2 {
    DWORD     cbSize;           /* +0x00  must be DPSESSIONDESC2_SIZE (0x50) */
    DWORD     dwFlags;          /* +0x04 */
    LOCO_GUID guidInstance;     /* +0x08  filled from received session data */
    LOCO_GUID guidApplication;  /* +0x18  = LOCO_DIRECTPLAY_GUID */
    DWORD     dwMaxPlayers;     /* +0x28 */
    DWORD     dwCurrentPlayers; /* +0x2C */
    union {
        LPSTR  lpszSessionNameA;  /* +0x30 */
        LPWSTR lpszSessionNameW;
    };
    union {
        LPSTR  lpszPasswordA;     /* +0x34 */
        LPWSTR lpszPasswordW;
    };
    DWORD     dwReserved1;      /* +0x38 */
    DWORD     dwReserved2;      /* +0x3C */
    DWORD     dwUser1;          /* +0x40 */
    DWORD     dwUser2;          /* +0x44 */
    DWORD     dwUser3;          /* +0x48 */
    DWORD     dwUser4;          /* +0x4C */
} LOCO_DPSESSIONDESC2; /* total size = 0x50 */

/*
 * GameNetState — main network subsystem state object.
 * Size: at least 0x15c4 bytes.
 *
 * Accessed by raw byte-offset arithmetic in the decompiled code.
 * Field names are reconstructed from context.
 *
 * Layout (selected fields — many padding regions omitted):
 *   +0x000  void*                vtable                (PTR_FUN_00479168)
 *   +0x001  BYTE                 abort_flag
 *   +0x018  char[0x3F4]          preconfigured_session_name
 *   +0x40c  LOCO_THREAD_HANDLE   thread_handle
 *   +0x418  BYTE                 thread_running
 *   +0x498  char[]               session_name  (-> DPSESSIONDESC2.lpszSessionNameA)
 *   +0x920  DWORD                reset_field   (cleared on MSG_TYPE_CONNECT)
 *   +0x924  DPID                 local_player_id
 *   +0x92c  LPVOID               session_desc_buf  (Win32: GlobalAlloc; Linux: malloc)
 *   +0x930  BYTE                 owns_session_buf  (0=must free, 1=external)
 *   +0x938  HWND                 main_window
 *   +0x93c  HINSTANCE            module_instance
 *   +0x940  void(*)(self,msg)    error_callback
 *   +0x944  BYTE                 enable_error_dialog
 *   +0x945  char[]               last_error_msg
 *   +0xd48  HRESULT              last_dp_hresult
 *   +0xd4c  void(*)(self)        idle_callback
 *   +0xd50  BYTE                 session_connected
 *   +0xd54  DWORD                remote_data_buf_size
 *   +0xd58  void*                remote_player_data_buf
 *   +0xd5c  DWORD                send_queue_flag
 *   +0x1588 LOCO_NET_IFACE       net_iface   (IDirectPlay4A* / ENetHost*)
 *   +0x158c LOCO_DPSESSIONDESC2  session_desc  (80 bytes inline)
 *   +0x15e8 DWORD                capability_flags  (bit 0x10000 = use SendEx)
 */
typedef struct _GameNetState {
    void     *vtable;
    BYTE      abort_flag;
    char      _pad_002_017[0x16];
    char      preconfigured_session_name[0x3F4]; /* +0x018 */
    LOCO_THREAD_HANDLE thread_handle;             /* +0x40c */
    char      _pad_410_417[0x08];
    BYTE      thread_running;                     /* +0x418 */
    char      _pad_419_497[0x7F];
    char      session_name[0x488];                /* +0x498 */
    DWORD     reset_field;                        /* +0x920 */
    DPID      local_player_id;                    /* +0x924 */
    char      _pad_928_92b[0x04];
    LPVOID    session_desc_buf;                   /* +0x92c */
    BYTE      owns_session_buf;                   /* +0x930 */
    char      _pad_931_937[0x07];
    HWND      main_window;                        /* +0x938 */
    HINSTANCE module_instance;                    /* +0x93c */
    void    (*error_callback)(struct _GameNetState *, const char *); /* +0x940 */
    BYTE      enable_error_dialog;                /* +0x944 */
    char      last_error_msg[0x403];              /* +0x945 */
    /* Fields at +0xd48 onward accessed by raw pointer arithmetic only. */
    /* +0xd48  HRESULT last_dp_hresult              */
    /* +0xd4c  void(*idle_callback)(GameNetState*)  */
    /* +0xd50  BYTE   session_connected              */
    /* +0xd54  DWORD  remote_data_buf_size           */
    /* +0xd58  void*  remote_player_data_buf         */
    /* +0xd5c  DWORD  send_queue_flag                */
    /* +0x1588 LOCO_NET_IFACE  net_iface             */
    /* +0x158c LOCO_DPSESSIONDESC2 session_desc      */
    /* +0x15e8 DWORD  capability_flags               */
} GameNetState;

/*
 * NetworkMessageHeader — common header for all protocol messages.
 * Full payload layout varies by msg_type; see MSG_TYPE_* constants.
 */
typedef struct _NetworkMessageHeader {
    WORD  msg_type;      /* +0x00  protocol message type (MSG_TYPE_*) */
    WORD  proto_version; /* +0x02  must equal PROTO_VERSION (300 / 0x12c) */
    /* payload follows at +0x04 */
} NetworkMessageHeader;

/*
 * NetworkMsg_PlayerName — type 0x03 and 0x103.
 */
typedef struct _NetworkMsg_PlayerName {
    WORD  msg_type;                /* +0x00 */
    WORD  proto_version;           /* +0x02 = PROTO_VERSION */
    DWORD reserved;                /* +0x04 */
    DWORD flags;                   /* +0x08 */
    char  _pad[0x18];              /* +0x0c..+0x23 */
    char  name[NAME_MAX_LEN + 1];  /* +0x24 player name, capped at NAME_MAX_LEN */
} NetworkMsg_PlayerName;

/*
 * NetworkMsg_Status — type 0x05.  8-byte payload.
 */
typedef struct _NetworkMsg_Status {
    WORD  msg_type;      /* +0x00 */
    WORD  proto_version; /* +0x02 */
    DWORD reserved;      /* +0x04 */
    DWORD data;          /* +0x08 */
} NetworkMsg_Status;

/*
 * NetworkMsg_SessionInfo — type 0x32.
 * Authoritative "session accepted" response that terminates the receive loop.
 */
typedef struct _NetworkMsg_SessionInfo {
    WORD      msg_type;        /* +0x00 = MSG_TYPE_SESSION_INFO (0x32) */
    WORD      proto_version;   /* +0x02 = PROTO_VERSION */
    WORD      count;           /* +0x04 */
    WORD      data_len;        /* +0x06 */
    LOCO_GUID session_guid;    /* +0x08 instance GUID (16 bytes) */
    char      session_name[1]; /* +0x18 variable length */
} NetworkMsg_SessionInfo;

/*
 * ReceiveResult — returned by DP_ReceiveDispatch.
 * Heap-allocated (Win32: HeapAlloc; Linux: malloc).
 * Caller must free both result->buffer and the ReceiveResult itself.
 */
typedef struct _ReceiveResult {
    DWORD  status;  /* +0x00  status/flags (0 = normal result) */
    void  *buffer;  /* +0x04  pointer to message buffer or secondary descriptor */
} ReceiveResult; /* size = 8 bytes */

/* =========================================================================
 * Platform-abstraction macros (networking)
 * ========================================================================= */

#ifdef LOCO_LINUX

/*
 * LOCO_SLEEP_MS(ms) — sleep for ms milliseconds.
 * Win32: Sleep(ms)  Linux: SDL_Delay(ms)
 */
#  define LOCO_SLEEP_MS(ms)   SDL_Delay(ms)

/*
 * LOCO_THREAD_ALIVE(tid) — test whether a thread is still running.
 * Win32: WaitForSingleObject(tid,0)==WAIT_TIMEOUT
 * Linux: pthread_kill(tid,0)==0
 */
#  define LOCO_THREAD_ALIVE(tid)  (pthread_kill((tid), 0) == 0)

/*
 * LOCO_THREAD_CLOSE(tid) — join/close a thread.
 * Win32: CloseHandle(tid)  Linux: pthread_join(tid, NULL)
 */
#  define LOCO_THREAD_CLOSE(tid)  pthread_join((tid), NULL)

/*
 * LOCO_RECV_GROW(ptr, size) — grow the receive buffer.
 * Win32: HeapFree + HeapAlloc(size += 0x7ff)
 * Linux: realloc with doubled size
 */
#  define LOCO_RECV_GROW(ptr, size) \
    do { (size) *= 2; (ptr) = realloc((ptr), (size)); } while (0)

/*
 * LOCO_SHOW_ERROR(win, title, msg) — display a modal error.
 * Win32: MessageBoxA(hwnd, msg, title, MB_TOPMOST)
 * Linux: SDL_ShowSimpleMessageBox or fprintf(stderr)
 */
#  define LOCO_SHOW_ERROR(win, title, msg) \
    do { \
        if ((win) != NULL) \
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, (title), (msg), (win)); \
        else \
            fprintf(stderr, "[LOCO ERROR] %s: %s\n", (title), (msg)); \
    } while (0)

/*
 * LOCO_FORMAT_STRING(buf, size, fmt, ...) — format a string.
 * Win32: wsprintfA  Linux: snprintf
 */
#  define LOCO_FORMAT_STRING(buf, size, fmt, ...) \
    snprintf((buf), (size), (fmt), ##__VA_ARGS__)

#else /* WIN32 */

#  define LOCO_SLEEP_MS(ms)         Sleep(ms)
#  define LOCO_THREAD_ALIVE(tid)    (WaitForSingleObject((tid), 0) == WAIT_TIMEOUT)
#  define LOCO_THREAD_CLOSE(tid)    CloseHandle(tid)
#  define LOCO_RECV_GROW(ptr, size) \
    do { HANDLE _hp = GetProcessHeap(); \
         (size) += 0x7ff; \
         HeapFree(_hp, 0, (ptr)); \
         (ptr) = HeapAlloc(_hp, 0, (size)); } while (0)
#  define LOCO_SHOW_ERROR(win, title, msg) \
    MessageBoxA((win), (msg), (title), MB_TOPMOST)
#  define LOCO_FORMAT_STRING(buf, size, fmt, ...) \
    wsprintfA((buf), (fmt), ##__VA_ARGS__)

#endif /* LOCO_LINUX */

/* =========================================================================
 * Function declarations — window management subsystem
 * ========================================================================= */

/*
 * FUN_00406480 — CGWND_SetupDisplay  (0x00406480)
 * Pre-window setup: desktop dimensions, window position from lego.ini.
 * WIN32: GetDesktopWindow, GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN), SetRect
 * LINUX: SDL_GetDisplayBounds(0,&r) / SDL_GetCurrentDisplayMode
 */
void FUN_00406480(CGWND *self);

/*
 * FUN_004068d0 — CGWND_LoadConfig  (0x004068d0)
 * Registry / INI path discovery and install-path persistence.
 * WIN32: RegOpenKeyExA, RegQueryValueExA, RegCreateKeyExA, RegSetValueExA, RegCloseKey
 * LINUX: fopen(SDL_GetPrefPath(...)), plain text / INI config
 */
BOOL FUN_004068d0(void);

/*
 * FUN_00406680 — CGWND_CheckDisplayCaps  (0x00406680)
 * Display capability pre-flight check (bit depth, palette mode).
 * WIN32: GetDC, GetDeviceCaps(RASTERCAPS/BITSPIXEL), ReleaseDC,
 *        GetSystemMetrics(SM_SAMEDISPLAYFORMAT), MessageBoxA
 * LINUX: SDL_GetCurrentDisplayMode; SDL_ShowSimpleMessageBox
 */
UINT FUN_00406680(CGWND *self);

/*
 * FUN_00406ed0 — CGWND_RegisterAndCreateWindow  (0x00406ed0)
 * Main window creation: RegisterClassA + CreateWindowExA, fullscreen.
 * WIN32: LoadIconA, RegisterClassA, CreateWindowExA, GetClientRect
 * LINUX: SDL_LoadBMP + SDL_SetWindowIcon; SDL_CreateWindow(FULLSCREEN_DESKTOP)
 */
BOOL FUN_00406ed0(CGWND *self);

/*
 * FUN_00406ba0 — GameLoop_Setup  (0x00406ba0)
 * Subsystem orchestration and 35 fps timer start.
 * WIN32: CreateEventA, timeBeginPeriod(14), timeSetEvent(28,14,cb,0,1)
 * LINUX: sem_init / eventfd; SDL_AddTimer(28, cb, NULL)
 */
int FUN_00406ba0(CGWND *self);

/*
 * FUN_004085e0 — PumpFilterMouse  (0x004085e0)
 * Mini message pump for heavy loading; filters mouse events.
 * WIN32: PeekMessageA(PM_NOREMOVE/PM_REMOVE), TranslateMessage, DispatchMessageA
 * LINUX: SDL_PeepEvents filtering SDL_MOUSE* events
 */
void FUN_004085e0(char blockMouse);

/*
 * FUN_00408350 — SceneInit / DeferredStartup  (0x00408350)
 * Deferred startup: installs 150 ms timer, blocks input, forces repaint.
 * WIN32: SetTimer(0x47,150), EnableWindow(FALSE), InvalidateRect, UpdateWindow
 * LINUX: SDL_AddTimer(150,...); loading flag; SDL_RenderPresent
 */
void FUN_00408350(HWND hWnd);

/*
 * FUN_004086f0 — GameStateExit  (0x004086f0)
 * Game-mode dispatcher: cancel timer, post state transitions, stop audio.
 * WIN32: KillTimer, PostMessageA(0x406), wsprintfA("Layouts\\%s"), PlaySoundA(NULL)
 * LINUX: SDL_RemoveTimer; SDL_PushEvent(custom); snprintf("Layouts/%s"); Mix_HaltChannel(-1)
 */
void FUN_004086f0(int mode);

/*
 * FUN_00407ae0 — ScrollSync  (0x00407ae0)
 * World scroll position: updates horizontal scrollbar widget.
 * WIN32: SetScrollRange(SB_HORZ,0,nMax,FALSE), SetScrollPos(SB_HORZ,pos,TRUE)
 * LINUX: plain integer; custom scrollbar sprite
 */
void FUN_00407ae0(HWND hWnd, int nMax, int pos);

/*
 * FUN_004077a0 — CGWND_Shutdown  (0x004077a0)
 * Shutdown: persist config, join worker thread, destroy subsystems, cancel timer.
 * WIN32: Sleep(100), CloseHandle(hEvent), timeKillEvent, timeEndPeriod(14)
 * LINUX: pthread_join; close/sem_destroy; SDL_RemoveTimer
 */
void FUN_004077a0(CGWND *self);

/*
 * FUN_00408130 — SetGameState  (0x00408130)
 * Display mode switch state machine (modes 0–0xa).
 * WIN32: PostMessageA(hwnd,WM_QUIT,0,0)  [mode 0xa]
 * LINUX: SDL_PushEvent({.type=SDL_QUIT})
 */
void FUN_00408130(int newMode);

/*
 * WndProc — 0x004618c0
 * Main window procedure registered as WNDCLASSA.lpfnWndProc.
 * WIN32: LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM)
 * LINUX: handled by SDL_PollEvent switch in Platform_ProcessEvents
 */
#ifndef LOCO_LINUX
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

/*
 * FUN_0045e1e0 — RequestRenderFlip  (0x0045e1e0)
 * Sends synchronous flip/present request via SendMessageA(0x407).
 * WIN32: SendMessageA(hwnd, 0x407, flags&0xFF, 0)
 * LINUX: SDL_RenderPresent(g_sdlRenderer) directly
 */
void FUN_0045e1e0(int flipFlags);

/*
 * FUN_00408b20 — WidgetCtor  (0x00408b20)
 * UI widget constructor: allocates sprite button sub-objects.
 * LINUX: SDL_CreateTextureFromSurface(renderer, IMG_Load(path))
 */
void *FUN_00408b20(void *self);

/*
 * FUN_00408d10 — WidgetDtor  (0x00408d10)
 * UI widget destructor: frees child lists and sprite objects.
 * LINUX: SDL_DestroyTexture per sprite
 */
void FUN_00408d10(void *self);

/*
 * FUN_004091a0 — WidgetLayout  (0x004091a0)
 * UI control layout calculation — pure arithmetic, no Win32 calls.
 * LINUX: direct SDL_Rect arithmetic
 */
void FUN_004091a0(void *self, int baseX);

/*
 * FUN_004094b0 — ListboxPaint  (0x004094b0)
 * Listbox paint handler: text measurement and colour-coded item drawing.
 * WIN32: GetDC wrapper, DrawTextA(DT_CALCRECT), SetTextColor, DirectDraw Blt
 * LINUX: SDL_Renderer; TTF_SizeText; SDL_SetRenderDrawColor; SDL_RenderPresent
 */
void FUN_004094b0(void *self, void *itemList);

/*
 * FUN_00409770 — TextWidgetDraw  (0x00409770)
 * Single-line text widget draw: DirectDraw BltFast + GDI text.
 * WIN32: IDirectDrawSurface::BltFast, SelectObject(hFont), DrawTextA(DT_CALCRECT)
 * LINUX: SDL_ttf TTF_SizeText; SDL_RenderCopy
 */
void FUN_00409770(void *self);

/* =========================================================================
 * Function declarations — networking subsystem (win32_platform.c)
 * ========================================================================= */

/*
 * DP_JoinSession — 0x00460360
 * Joins/opens a Lego Loco DirectPlay (Win32) / ENet (Linux) session.
 * Returns 1 on success, DPERR_JOIN_FATAL_RETURN on specific error, 0 on failure.
 */
int DP_JoinSession(GameNetState *obj);

/*
 * DP_ReceiveDispatch — 0x004606d0
 * Polls for incoming messages and dispatches by type.
 * Returns a heap-allocated ReceiveResult* on a completed event, NULL otherwise.
 * Caller must free result->buffer and the result itself.
 */
ReceiveResult *DP_ReceiveDispatch(GameNetState *obj);

/*
 * DP_SendMessage — 0x00460d40
 * Sends a protocol message; stamps PROTO_VERSION at header offset +2.
 * Returns param_flags on success, 0 on error.
 * Original calling convention: __thiscall (this=obj, playerID, msgBuf, msgLen, flags)
 */
uint32_t DP_SendMessage(GameNetState *obj, uint32_t playerID,
                         void *msgBuf, uint32_t msgLen, uint32_t param_flags);

/*
 * Platform_DisplayError — 0x00460ea0
 * Composes and displays an error from a string resource ID and optional extra string.
 * Uses error_callback at obj+0x940 if set; otherwise MessageBoxA / SDL_ShowSimpleMessageBox.
 */
void Platform_DisplayError(GameNetState *obj, uint32_t resourceID,
                            const char *param_extra);

/*
 * NetObject_Construct — 0x00461610
 * Constructor/initialiser for a network thread-wrapper object.
 * Sets vtable; zeroes handle and status fields.  No Win32 dependencies.
 */
void NetObject_Construct(void **param_1);

/*
 * NetObject_Destroy — 0x00461640
 * Destructor: resets vtable; closes primary thread handle at +0x40c.
 * Win32: CloseHandle  Linux: pthread_join after pthread_cancel
 * Returns: this pointer.
 */
void *NetObject_Destroy(void **self, int param_1);

/*
 * NetObject_ResetHandle — 0x00461690
 * Closes the secondary handle slot at [0x103].
 * Win32: CloseHandle  Linux: pthread_join or close(fd)
 */
void NetObject_ResetHandle(void **param_1);

/*
 * Thread_SetPriorityIfRunning — 0x004616c0
 * Checks thread liveness, then sets scheduling priority if running.
 * Win32: WaitForSingleObject(h,0) + SetThreadPriority
 * Linux: pthread_kill(tid,0)==0  + pthread_setschedparam / setpriority
 * Returns: non-zero on success, 0/FALSE if thread has exited.
 */
int Thread_SetPriorityIfRunning(void **self, int param_priority);

/* =========================================================================
 * Linux-only helper function declarations
 * ========================================================================= */

#ifdef LOCO_LINUX

/*
 * loco_win32_priority_to_nice
 * Maps Win32 THREAD_PRIORITY_* to a POSIX nice value (-19..+19).
 * Mapping: LOWEST(-2)->+4, BELOW_NORMAL(-1)->+2, NORMAL(0)->0,
 *          ABOVE_NORMAL(1)->-2, HIGHEST(2)->-4
 */
int loco_win32_priority_to_nice(int win32_priority);

/*
 * loco_load_string
 * Replacement for LoadStringA: returns a static string from the string table,
 * indexed by (resourceID - LOCO_STR_BASE).  Returns "" for unknown IDs.
 */
const char *loco_load_string(uint32_t resourceID);

/*
 * loco_discover_sessions_and_pick
 * Replacement for DialogBoxParamA session-picker (resource DIALOG_SESSION_PICKER).
 * UDP broadcast discovery: sends LOCO_DIRECTPLAY_GUID magic token, collects
 * responses, presents list via SDL UI or CLI.
 */
void loco_discover_sessions_and_pick(GameNetState *obj);

#endif /* LOCO_LINUX */

/* =========================================================================
 * Compile-time assertions
 * ========================================================================= */

#ifdef __STDC_VERSION__
#  if __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(LOCO_DPSESSIONDESC2) == DPSESSIONDESC2_SIZE,
               "LOCO_DPSESSIONDESC2 size mismatch — wire format broken");
_Static_assert(sizeof(ReceiveResult) == 8,
               "ReceiveResult size mismatch — expected 8 bytes");
#  endif
#endif

#endif /* LOCO_WIN32_PLATFORM_H */
