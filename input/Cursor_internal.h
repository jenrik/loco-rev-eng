// Status: INTEGRATED
/* Cursor_internal.h — Shared internals for Cursor implementation files */
#pragma once

#include "Cursor.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* Win32 API declarations.
 * NOTE: Most of these are also in stubs/windows.h. The _WIN32 block
 * remains as a self-contained fallback. TODO [PROGRESS.md]: Consolidate
 * all Win32 declarations into stubs/windows.h; remove per-file duplicates.
 * Non-Windows builds use sdl3_window.h. */
#ifdef _WIN32
extern "C" {
HBRUSH CreateSolidBrush(uint32_t);
BOOL DeleteObject(HGDIOBJ);
HWND GetDesktopWindow(void);
BOOL GetClientRect(HWND, RECT*);
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void*);
HWND GetCapture(void);
HWND SetCapture(HWND);
BOOL ReleaseCapture(void);
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef void* HLOCAL;

int ShowCursor(BOOL);
BOOL ShowWindow(HWND, int);
BOOL DestroyWindow(HWND);
void PostQuitMessage(int);
UINT_PTR SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
BOOL KillTimer(HWND, UINT_PTR);
void PostMessageA(HWND, UINT, WPARAM, LPARAM);
void GetCursorPos(POINT*);
LONG SetWindowLongA(HWND, int, LONG);
HWND SetFocus(HWND);
void SetWindowTextA(HWND, const char*);
void OutputDebugStringA(const char*);
BOOL ClientToScreen(HWND, POINT*);
BOOL SetRect(RECT*, int, int, int, int);
BOOL CopyRect(RECT*, const RECT*);
BOOL OffsetRect(RECT*, int, int);
int UnionRect(RECT*, const RECT*, const RECT*);
BOOL IntersectRect(RECT*, const RECT*, const RECT*);
BOOL PtInRect(const RECT*, POINT);
void Sleep(DWORD);
void ExitProcess(UINT);
BOOL EnableWindow(HWND, BOOL);
BOOL FillRect(HDC, const RECT*, HBRUSH);
int DrawTextA(HDC, LPCSTR, int, RECT*, UINT);
BOOL DrawEdge(HDC, RECT*, UINT, UINT);
int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
BOOL GetOpenFileNameA(void*);
HANDLE CreateFileA(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
DWORD GetFileSize(HANDLE, DWORD*);
BOOL CloseHandle(HANDLE);
DWORD GetLastError(void);
DWORD FormatMessageA(DWORD, const void*, DWORD, DWORD, LPSTR, DWORD, void*);
HLOCAL LocalFree(HLOCAL);
HGDIOBJ GetStockObject(int);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
COLORREF SetTextColor(HDC, COLORREF);
int SetBkMode(HDC, int);
COLORREF SetBkColor(HDC, COLORREF);
int wsprintfA(char*, const char*, ...);
HICON LoadIconA(HINSTANCE, LPCSTR);
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
}
#endif /* _WIN32 */

#ifndef _WIN32
/* Non-Windows: sdl3_window.h provides SDL3-backed equivalents of the above */
#include "sdl3_window.h"
#endif /* !_WIN32 */

/* Game-specific and CRT function declarations (all platforms) */

/* C++ linkage — these are C++ symbols in the binary, not C functions */
void* operator_new(size_t);
void GLOBAL_free(void*);
int CRT_atoi(const char*);
int CRT_rand(void);
void CRT_free(void*);

/* ================================================================ */
/* C-linkage: DirectDraw/DirectX platform helpers (COM ABI)          */
/*   These are genuine C-linkage symbols from the binary.             */
/* ================================================================ */
extern "C" {
size_t strlen(const char*);
void* memcpy(void*, const void*, size_t);
/* The binary calls DDRAW_UnlockPrimary() with NO arguments everywhere
 * (e.g. 0x414EF0, 0x414290, 0x414C20, 0x414BB0); graphics/DDRAW.h also
 * declares `void __cdecl DDRAW_UnlockPrimary(void)`. The previous
 * HWND-parameter form was an unsupported transcription. */
void DDRAW_UnlockPrimary(void);
int DDRAW_SetSurfaceFormat(void*, int);
int DDRAW_RestoreSurfaces(void*, void*);
void DDRAW_GetSurfaceWidthHeight(void* surface, uint16_t* out_h, uint16_t* out_w);
void* CRT_wcsstr(const uint8_t*, const uint16_t*);
void Sprite_Destroy(void*);  /* declared extern "C" in ButtonSprite.h */

/* The following were previously declared with default C++ linkage below,
 * causing call-0 landmines: their only real definitions (link_stubs.cpp /
 * defsym_stubs.cpp) are extern "C" (unmangled), so a C++-mangled caller
 * declaration never bound to them. See docs/landmine-sweep-worklist.md
 * "Cursor family" entry. */
void* WIN32_StreamOpenFile(void*, const char*, uint32_t, uint32_t, uint32_t);
void WIN32_StreamRead(void*, void*, int32_t);
/* Real def (link_stubs.cpp) takes a single this-pointer-only param — no
 * second int32_t. Confirmed against 0x443440: loops surface_cache[256] with
 * ECX = this and no pushed stack args. */
void DPLAY_LeaveSession(void*);
}

/* ================================================================ */
/* C++-linkage: game engine helpers                                  */
/*   These are C++ ABI symbols from the binary — use C++ linkage.    */
/*   NOTE: These are transitional bridge declarations. The eventual  */
/*   intent is to replace them with direct C++ method calls.         */
/* ================================================================ */
#include "../ui/UIPANEL_Surface.h"
#include "../network/NetworkPlayerList.h"  /* real g_dplay singleton + typed methods */

void* ResourceManager_GetById(void*, int);
void Sprite_Init(void*);
void Sprite_SetState(void*, int, int*);
int UI_CreateFullWindow(void*, int, HWND, int, int, int, int, HMENU, HICON, UINT);
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t) — was declared
 * uniformly int32_t, which doesn't match the real mixed uint32_t/int32_t
 * shape (call-0 landmine). */
bool UIPANEL_Blit(void* srcSurf, uint32_t sx, uint32_t sy, int32_t sw, uint32_t sh,
                   void* dstSurf, uint32_t dx, uint32_t dy, int32_t dw, uint32_t dh, uint32_t flags);
HDC UIPANEL_BeginPaint(void*);
void UIPANEL_EndPaint(void*);
void UIPANEL_EndPaintEx(void*, HWND, int, uint8_t, RECT*);
void* UIPANEL_CreateSurface(void*);
size_t UIPANEL_Surface_Size();  /* graphics/LOCOBITMAP.cpp — real sizeof(UIPANEL_Surface) */
void UIPANEL_UnlockSurface(void*);
void FormatResourceString(void*, UINT, LPSTR, int);
/* DPLAY_EnumeratePlayers and NET_GetOrCreateSurface are now typed
 * NetworkPlayerList methods (see ../network/NetworkPlayerList.h) — callers
 * use g_dplay->EnumeratePlayers() / g_dplay->GetOrCreateSurface(...)
 * directly rather than these free-function forms. */
int NET_FindPlayer(int, int);
uint16_t NET_UploadAsset(int, char*);
void WIN32_FatalError(void);
void WIN32_PostQuit(void);              /* 0x463670 — real body in
                                            core/CGWND.cpp */
/* Canonical signature (ResourceManager.h, 0x447930).  The old `int`
 * form hijacked overload resolution in TUs including both headers,
 * binding calls to the never-defined _Z9PlaySoundi instead of the
 * real _Z9PlaySoundj (undefined-symbol runtime crash with the
 * ignore-all link). */
void PlaySound(unsigned int);
void INPUT_SwitchToLocomotiveTab(void*, int);
void PlaySoundAt(int, int, int, int);
void PlaySoundFile(char*, int, int, int);
int HelpWnd_PlayNarration(void*, int, int);
int* AssetMgr_LoadFile(void*, uint8_t*, int*);
/* WIN32_StreamOpenFile/WIN32_StreamRead moved to the extern "C" block above
 * — their only real definitions are extern "C". */
/* Game_SetScreenMode's real (and only) definition is the loud stub at
 * shared/stubs_impl.cpp:441, `(void*, char, char, char)`. `char` and
 * `unsigned char`/`uint8_t` are distinct Itanium-mangled types, so the
 * previous `uint8_t` params here never bound to it. */
void Game_SetScreenMode(void*, char, char, char);
/* Real def (shared/defsym_stubs.cpp) is the single-char "loading
 * transition pump" overload, distinct from the (void*, uint8_t) main-loop
 * pump in core/CGWND_sdl3.cpp. */
void CGWND_PumpMessages(char);
int DPLAY_CreatePlayer(void* record);
/* NOTE on DPLAY_RenderPlayer: the binary call site at 0x418A9C pushes NINE
 * stack args (hdc, player, surface, left, top, right, bottom, hWnd,
 * this+0x138) and the callee (NetworkPlayerList::RenderPlayer, 0x4437C0)
 * does RET 0x24 (9 dwords) — a real 9-arg reconstruction is a separate,
 * larger task owned by the network subsystem (see NetworkPlayerList.h,
 * which already implements an 8-arg approximation). Cursor's call site
 * targets the free-function no-op stub (shared/link_stubs.cpp), matching
 * its real signature exactly rather than attempting the 9-arg fidelity
 * fix or binding into the already-integrated (but RECT-aliasing-fragile)
 * typed method — see docs/landmine-sweep-worklist.md "Cursor family". */
void DPLAY_RenderPlayer(void* dplay, void* hdcVal, int32_t player,
                         void* surface, int32_t x, int32_t y, uint32_t w,
                         RECT* rect);
size_t WIN32_Stream_Size();  /* resources/Win32Stream.cpp — real sizeof(WIN32_Stream) */
/* Canonical signature/size helper: resources/Win32StreamMem.h. Declared
 * locally here (forward-declared class, not a full #include) rather than
 * including that header directly: this file is included by every
 * Cursor*.cpp translation unit, some of which transitively pull in
 * graphics/sdl3_window.h (SDL3 Win32-windowing shim); Win32StreamMem.h
 * transitively includes resources/StreamObject.h's real <windows.h>
 * (stubs/windows.h), and the two headers declare several Win32 functions
 * (SetRect/GetCursorPos/SetTimer/KillTimer/MSG) with incompatible
 * signatures — a real, pre-existing header-organization conflict between
 * those two subsystems, out of this pass's scope to resolve tree-wide.
 * input/Cursor.cpp itself includes the full header directly (see its own
 * top-of-file comment) since it needs WNDPROC_Stream's complete type for
 * real construction/state-bits access, and does not transitively include
 * graphics/sdl3_window.h. */
class WNDPROC_Stream;
WNDPROC_Stream* WNDPROC_StreamFromMemory(void* stream, char* data, int size, int mode);
size_t WIN32_MemoryStream_Size();

/* ================================================================ */
/* C++ linkage — game helpers originally compiled as C++ symbols     */
/*   NOTE: UI_WindowBase_* bridge declarations removed — Cursor      */
/*   code now uses direct C++ method calls (UI_WindowBase::hide(),   */
/*   UI_WindowBase::show(), UI_WindowBase::base_destructor()).       */
/* ================================================================ */

/* Globals — typed where types are known */
extern Cursor* g_cursor;
extern void*   g_resmgr;        /* ResourceManager* */
extern void*   g_asset_mgr;     /* AssetMgr* */
extern char    g_install_path[];
extern void*   g_ddraw;         /* IDirectDraw4* — COM platform object */
extern void*   _g_backbuffer;   /* IDirectDrawSurface4* — COM platform object */
extern void*   _g_primary_surface; /* IDirectDrawSurface4* — COM platform object */
/* NOTE: _g_dplay is a SEPARATE global from the real NetworkPlayerList
 * singleton `g_dplay` (network/NetworkPlayerList.h, constructed by
 * core/GameLoop.cpp at startup) — it is initialized to nullptr once
 * (shared/link_stubs.cpp) and never assigned anywhere, so it is always
 * null. This is a distinct landmine class (a duplicate/orphaned global
 * from the 32-to-64-bit port, not a call-0 or undersized-allocation
 * bug) — see docs/landmine-sweep-worklist.md. Do not use it for real
 * NetworkPlayerList access; use the typed `g_dplay` from
 * network/NetworkPlayerList.h instead (already included below). */
extern void*   _g_dplay;        /* NetworkPlayerList* at 0x4FD3B0 (not IDirectPlay4) — always null, see NOTE above */
extern int     _g_cursor_refcount;
extern void*   _g_cursor_back;  /* IDirectDrawSurface4* — COM platform object */

/* The real binary global at 0x4FD3AC is the Netman singleton, declared
 * `extern Netman* _g_netman;` in network/Netman.h. The legacy `g_netman`
 * name was a host-stub symbol (shared/stubs_impl.cpp) that never aliased
 * the real object; Cursor code now uses _g_netman directly.
 *
 * Forward declaration only — Cursor reads Netman's m_gameMode (+0x7C4),
 * m_mySlotIndex (+0x7D0) and m_slots[9] (+0x518) through verified raw
 * offsets (see Cursor_impls.cpp update_network_names). */
class Netman;
extern Netman* _g_netman;        /* 0x4FD3AC — Netman singleton pointer */
extern char    g_empty_string[];
extern void*   g_game;          /* Game* */
extern void*   g_player_config; /* PlayerConfig* */
extern uint8_t g_is_fullscreen;
extern int32_t g_client_width;
extern int32_t g_client_height;
extern int32_t g_viewport_x;
extern int32_t g_viewport_y;
extern void*   g_town;          /* Town* */
extern void*   g_postcard;      /* PostcardAlbum* */
extern void*   g_postcard_send; /* PostcardPreviewWindow* */
extern void*   g_ui_main;       /* EditWindow* */
extern void*   g_main_window;   /* UI_WindowBase* */
extern void*   g_font_small;    /* HFONT — GDI object */
extern int     g_surface_bpp;
extern void*   g_audio_mgr;     /* AudioMgr* */

/* Shared internal functions */
void Cursor_UnlockAllSurfaces(void);

/* COM IUnknown::Release() via vtable slot [2] on an opaque DirectDraw
 * surface. DirectDraw surfaces are platform COM objects, not decompiled
 * classes — literal vtable dispatch is the documented ABI (AGENTS.md
 * permits this for opaque COM surfaces). */
static inline void Cursor_ComSurfaceRelease(void* surface) {
    void** vtbl = *reinterpret_cast<void***>(surface);
    using ReleaseSurface = void (*)(void*);
    reinterpret_cast<ReleaseSurface>(vtbl[2])(surface);
}

/* Surface vtable function pointer types (DirectDraw ABI — __stdcall convention) */
typedef int (__stdcall *SurfaceBlt_t)(void*, RECT*, void*, RECT*, uint32_t, void*);
typedef int (__stdcall *SurfacePollBlit_t)(void*, void*);
using SurfaceReleaseDc_t = void (*)(void*, void*);
using SurfaceFill_t = int (*)(void*, int, int, int, int, int*);
using SurfaceLegacyBlt_t = int (*)(void*, int*, void*, int*, int, void*);

static inline SurfaceBlt_t Cursor_SurfaceBlt(void* surface) {
    void** vtbl = *reinterpret_cast<void***>(surface);
    return reinterpret_cast<SurfaceBlt_t>(vtbl[0x14 / 4]);
}

/* vtable slot 26 (byte offset 0x68). In the IDirectDrawSurface4 ABI this
 * is ReleaseDC(surface, hdc) — the pre-render call in Cursor::render
 * (0x414C28) passes the hdc argument. (The previous transcription named
 * it "Lock"; the documented interface slot table says ReleaseDC.) */
static inline SurfaceReleaseDc_t Cursor_SurfaceReleaseDC(void* surface) {
    void** vtbl = *reinterpret_cast<void***>(surface);
    return reinterpret_cast<SurfaceReleaseDc_t>(vtbl[0x68 / 4]);
}

static inline SurfaceFill_t Cursor_SurfaceFill(void* surface) {
    void** vtbl = *reinterpret_cast<void***>(surface);
    return reinterpret_cast<SurfaceFill_t>(vtbl[5]);
}

static inline SurfaceLegacyBlt_t Cursor_SurfaceLegacyBlt(void* surface) {
    void** vtbl = *reinterpret_cast<void***>(surface);
    return reinterpret_cast<SurfaceLegacyBlt_t>(vtbl[5]);
}
#define BLIT_WAIT 0x1000000
#define BLIT_KEYSRC 0x8000
#define BLIT_KEYSRC_WAIT (BLIT_KEYSRC | BLIT_WAIT)

/* ================================================================== */
/* RESDATA vtable helpers                                              */
/*                                                                     */
/* NOTE: The RESDATA_ prefix on GetSurface/ReleaseSurface comes from  */
/* the RESDATA struct name (defined in shared/types.h), NOT from       */
/* Ghidra auto-label conventions. These are transitional bridge        */
/* helpers for Cursor's use of the RESDATA vtable.                     */
/*                                                                     */
/* TODO [PROGRESS.md]: Refactor RESDATA from POD struct with manual   */
/* vtable into a C++ class with virtual methods GetSurface() and       */
/* ReleaseSurface(). The literal vtable dispatch here (vtbl[1]/[2])    */
/* is an AGENTS.md §4 anti-pattern; acceptable as a transitional       */
/* bridge until RESDATA is refactored across all consumers.            */
/*                                                                     */
/* RESDATA is defined in shared/types.h as a POD struct with a vtable  */
/* pointer at +0x00. These helpers provide typed access to RESDATA's   */
/* vtable slots used by Cursor (other consumers may use different      */
/* signatures for the same slots depending on the RESDATA subclass).   */
/*                                                                     */
/* RESDATA vtable layout (Cursor-specific subset):                     */
/*   [0]  scalar deleting destructor                                  */
/*   [1]  GetSurface(resdata, flags, mode) → surface ptr              */
/*   [2]  ReleaseSurface(resdata)                                     */
/* ================================================================== */
static inline void* RESDATA_GetSurface(RESDATA* resdata, int flags, int mode) {
    void** vtbl = *reinterpret_cast<void***>(resdata);
    using GetSurface = void* (*)(void*, int, int);
    return reinterpret_cast<GetSurface>(vtbl[1])(resdata, flags, mode);
}
static inline void RESDATA_ReleaseSurface(RESDATA* resdata) {
    void** vtbl = *reinterpret_cast<void***>(resdata);
    using ReleaseSurface = void (*)(void*);
    reinterpret_cast<ReleaseSurface>(vtbl[2])(resdata);
}

