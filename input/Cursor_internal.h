// Status: INTEGRATED
/* Cursor_internal.h — Shared internals for Cursor implementation files */
#pragma once

#include "Cursor.h"
#include "../resources/ResourceObject.h"
#include "../resources/AssetArchive.h"
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
/* CRT_wcsstr was declared here as `void* CRT_wcsstr(const uint8_t*, const
 * uint16_t*)` — neither its extern "C" linkage nor its signature match the
 * real function (0x471480, `int32_t CRT_wcsstr(uint8_t*, uint8_t*)`, plain
 * C++ linkage — see game/Building.cpp's top-of-file comment); it also had
 * zero call sites in this file. Not a "genuine C-linkage symbol from the
 * binary" like its neighbors above; removed 2026-08-16. */
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
/* Real def: ui/UIPANEL.cpp:0x426B90 — the 2nd param is `int hdc`, not
 * `HWND` (HWND == void*). Was declared with an HWND 2nd param, mangling
 * to a distinct symbol from the real function, so every one of
 * input/Cursor_new_impls.cpp's 25 call sites through this header was a
 * silent-wrong-stub landmine binding to shared/stubs_impl.cpp's host
 * no-op instead of the real present pipeline (the identical landmine
 * already fixed in native/NETMAN_NetworkUI.c;
 * docs/landmine-sweep-worklist.md). */
void UIPANEL_EndPaintEx(void*, int, int, uint8_t, RECT*);
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
/* AssetMgr_LoadFile(void*, uint8_t*, int*) removed — g_asset_mgr is a real
 * AssetArchive value object (resources/AssetArchive.h); call sites now use
 * g_asset_mgr.LoadFile(...) directly. */
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
/* DPLAY_CreatePlayer(void* record) removed 2026-08-14 — its one real call
 * site, Cursor::init_network_player() (input/Cursor_impls.cpp), now
 * constructs a real DPlayManager directly (operator_new(sizeof(DPlayManager))
 * + placement-new + CreatePlayer()) instead of going through this
 * free-function facade, which bound to a no-op stub returning a garbage
 * `int` (the real DPlayManager::CreatePlayer() (0x442850) returns void). */
/* DPLAY_RenderPlayer free-function facade removed 2026-08-17: the real
 * 9-arg ABI (RET 0x24) is now fully resolved — see
 * NetworkPlayerList::RenderPlayer's doc comment (network/
 * NetworkPlayerList.h/.cpp) for the full object-model resolution.
 * Cursor::blit_edit_preview's real call site now calls
 * `g_dplay->RenderPlayer(...)` directly. */
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
/* g_asset_mgr — real AssetArchive value object, declared in
 * resources/AssetArchive.h (included above). */
extern char    g_install_path[];
extern void*   g_ddraw;         /* IDirectDraw4* — COM platform object */
extern void*   _g_backbuffer;   /* IDirectDrawSurface4* — COM platform object */
extern void*   _g_primary_surface; /* IDirectDrawSurface4* — COM platform object */
/* _g_dplay (a separate, permanently-null global from the real
 * NetworkPlayerList singleton `g_dplay`, network/NetworkPlayerList.h,
 * constructed by core/GameLoop.cpp at startup) removed 2026-08-17 — every
 * former reader now uses the typed `g_dplay` from
 * network/NetworkPlayerList.h instead (already included below). */
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
/* Matches the canonical declarations in game/PlayerConfig.h,
 * network/DPlayManager.h, network/Netman.h (2026-08-14 — these two were
 * previously declared void* and char[] here, a locally weaker type than
 * every other declaration of the same two globals tree-wide; unified once
 * input/Cursor_impls.cpp needed to #include network/DPlayManager.h
 * directly and the two conflicted at the type level, not just cosmetically). */
class PlayerConfig;
extern char          g_empty_string;    /* 0x4851D0 — empty string constant */
extern PlayerConfig* g_player_config; /* 0x4AA4A8 — PlayerConfig singleton */
extern uint8_t g_is_fullscreen;
extern int32_t g_client_width;
extern int32_t g_client_height;
extern int32_t g_viewport_x;
extern int32_t g_viewport_y;
/* g_town/g_cursor/g_postcard/g_postcard_send occupy one contiguous run of
 * four pointer-sized globals (0x4FD37C/0x4FD380/0x4FD384/0x4FD388) —
 * confirmed directly via get_xrefs_to on each address (2026-08-14), not
 * just by comment consensus: several other files' address comments for
 * this same cluster disagree with each other and with the confirmed
 * values (`core/GameLoop.cpp` has all three of town/cursor/postcard_send
 * rotated onto the wrong address; `town/Town.cpp` and `ui/HelpWnd.cpp`
 * each have their own, different, wrong value for g_cursor) — those are
 * pre-existing comment-only errors elsewhere in the tree, not fixed here
 * (out of scope: they don't affect linkage, `extern` binds by symbol name
 * regardless of what the trailing comment claims). Retyped from `void*`
 * to their strongest evidenced types, matching the g_player_config/
 * g_empty_string precedent above (only forward-declared here — these are
 * only ever used as pointers in Cursor code, never dereferenced through
 * their full type, so pulling in each class's full header is unneeded). */
class Game;
class Town;
class PostcardAlbum;
class PostcardPreviewWindow;
extern Game*                  g_game;          /* 0x4854C8 */
extern Town*                  g_town;          /* 0x4FD37C */
extern PostcardAlbum*         g_postcard;      /* 0x4FD384 */
extern PostcardPreviewWindow* g_postcard_send; /* 0x4FD388 */
extern void*   g_ui_main;       /* EditWindow* */
extern void*   g_main_window;   /* UI_WindowBase* */
extern void*   g_font_small;    /* HFONT — GDI object */
extern int     g_surface_bpp;
extern void*   g_audio_mgr;     /* AudioMgr* */

/* Shared internal functions */
void Cursor_UnlockAllSurfaces(void);

/* This header previously declared 5 raw-vtable-slot-dispatch helpers
 * (Cursor_ComSurfaceRelease/Cursor_SurfaceBlt/Cursor_SurfaceReleaseDC/
 * Cursor_SurfaceFill/Cursor_SurfaceLegacyBlt) for calling DirectDraw
 * surface methods (Release/Blt/ReleaseDC, all real slots already
 * identified in their own since-removed comments). Their claim that raw
 * dispatch was a permitted "opaque COM surface" ABI exception was true
 * only while no typed interface existed; platform/ddraw_interfaces.h's
 * real IDirectDrawSurface4 made that stale, since this shim's own
 * compiler-generated vtable order doesn't match those real ABI slot
 * numbers (would dispatch through the wrong method on a real object).
 * Removed 2026-08-14 — all 28 call sites now use
 * static_cast<IDirectDrawSurface4*>(surface)->Method(...) directly. */
#define BLIT_WAIT 0x1000000
#define BLIT_KEYSRC 0x8000
#define BLIT_KEYSRC_WAIT (BLIT_KEYSRC | BLIT_WAIT)

/* ================================================================== */
/* RESDATA vtable helpers                                              */
/*                                                                     */
/* NOTE: The RESDATA_ prefix on GetSurface/ReleaseSurface comes from  */
/* the RESDATA struct name (defined in shared/types.h), NOT from       */
/* Cursor's use of the RESDATA vtable, via real ResourceObject virtual       */
/* dispatch (resources/ResourceObject.h) -- RESDATA's 3-slot vtable         */
/* ([0]=dtor, [1]=Lock/GetSurface, [2]=Unlock/ReleaseSurface) is real C++    */
/* now, not a manually-indexed function-pointer array. Takes `void*` rather */
/* than `RESDATA*`: RESDATA itself stays an unmodeled POD struct (never the */
/* runtime type behind any resource pointer on this host-only-executing     */
/* build -- see resources/ResourceObject.h), so a `RESDATA*` parameter here */
/* would just be a misleading label on what's really always a              */
/* loco::assets::SpriteResource*.                                          */
/* ================================================================== */
static inline void* RESDATA_GetSurface(void* resdata, int flags, int mode) {
    return static_cast<ResourceObject*>(resdata)->Lock(flags, mode);
}
static inline void RESDATA_ReleaseSurface(void* resdata) {
    static_cast<ResourceObject*>(resdata)->Unlock();
}

