/*
 * Lego Loco (1998) - SDL3 Port
 * src/main_sdl3.cpp — Entry point that boots the ORIGINAL game code
 *
 * This replaces the Win32 CRT entry + WinMain chain:
 *   Original: entry() → WinMain(HINSTANCE, ...) → CGWND ctor → InitAllSubsystems → game loop
 *   SDL3:     main() → SDL3 init → DDraw bridge → CGWND ctor → InitAllSubsystems → game loop
 *
 * The decompiled C++ files contain the ACTUAL game logic from loco.exe.
 * We provide SDL3 implementations for the DirectX/Win32 APIs they call.
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

/* SDL3 backend (window, renderer, audio, input) */
#include "port/sdl3/sdl3_backend.h"

/* DirectX → SDL3 bridge (makes IDirectDraw4 etc. work) */
#include "decompiled_cpp/port/sdl3_compat.h"
#include "decompiled_cpp/port/sdl3_dsound_bridge.h"

/* Original game headers */
#include "core/CGWND.h"
#include "shared/types.h"

/* Forward declarations from bridge */
void SDL3_DDraw_Init(void);
void SDL3_DDraw_Shutdown(void);

/* =========================================================================
 * Win32 API stubs needed by the original game code
 * ========================================================================= */

extern "C" {
    /* Game.cpp imports — stubbed for SDL3 */
    int __stdcall ShowCursor(int show) { return show ? 1 : 0; }
    int __stdcall SetCursorPos(int x, int y) { (void)x; (void)y; return 1; }
    void* __stdcall SetCapture(void* w) { (void)w; return NULL; }
    int __stdcall ReleaseCapture() { return 1; }
    void* __stdcall LoadCursorA(void* inst, void* name) { (void)inst; (void)name; return NULL; }
    void* __stdcall LoadCursorFromFileA(const char* path) { (void)path; return NULL; }
    void* __stdcall SetCursor(void* cursor) { (void)cursor; return NULL; }
    int __stdcall GetCursorPos(void* pt) { (void)pt; return 0; }
    int __stdcall ClientToScreen(void* hwnd, void* pt) { (void)hwnd; (void)pt; return 1; }
    int __stdcall ScreenToClient(void* hwnd, void* pt) { (void)hwnd; (void)pt; return 1; }
    void* __stdcall WindowFromPoint(int x, int y) { (void)x; (void)y; return NULL; }
    int __stdcall SystemParametersInfoA(int a, int b, void* c, int d) { (void)a; (void)b; (void)c; (void)d; return 0; }
    int __stdcall GetPrivateProfileStringA(const char* s, const char* k, const char* d, char* b, int n, const char* f) {
        (void)s; (void)k; (void)d; (void)b; (void)n; (void)f; return 0;
    }
    int __stdcall GetPrivateProfileIntA(const char* s, const char* k, int d, const char* f) {
        (void)s; (void)k; (void)d; (void)f; return 0;
    }
    long __stdcall RegOpenKeyExA(void* a, const char* b, int c, int d, void* e) { (void)a;(void)b;(void)c;(void)d;(void)e; return 2; }
    long __stdcall RegQueryValueExA(void* a, const char* b, int* c, int* d, void* e, int* f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 2; }
    long __stdcall RegCloseKey(void* a) { (void)a; return 0; }
    long __stdcall RegCreateKeyExA(void* a, const char* b, int c, char* d, int e, int f, void* g, void* h, int* i) {
        (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i; return 2;
    }
    long __stdcall RegSetValueExA(void* a, const char* b, int c, int d, const char* e, int f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 2; }

    /* kernel32 imports */
    int __stdcall OutputDebugStringA(const char* s) { fprintf(stderr, "%s", s); return 0; }
    int __stdcall wsprintfA(char* buf, const char* fmt, ...);
    void* __stdcall GetModuleHandleA(const char* name) { (void)name; return NULL; }
    void* __stdcall LoadLibraryA(const char* name) { (void)name; return NULL; }
    void* __stdcall GetProcAddress(void* lib, const char* name) { (void)lib; (void)name; return NULL; }
    int __stdcall FreeLibrary(void* lib) { (void)lib; return 0; }

    /* GDI imports */
    void* __stdcall CreateSolidBrush(int color) { (void)color; return NULL; }
    int __stdcall DeleteObject(void* obj) { (void)obj; return 1; }
    int __stdcall PostMessageA(void* hwnd, int msg, int wp, int lp) { (void)hwnd;(void)msg;(void)wp;(void)lp; return 1; }

    /* CRT functions used by the decompiled code */
    void* __cdecl operator_new(size_t size);
    void  __cdecl GLOBAL_free(void* ptr);
    int   __cdecl CRT_mkdir(const char* path, int* err);
    void  __cdecl CRT_memset(char* buf, int val, int size);
    int   __cdecl CRT_strlen(const char* s);
    int   __cdecl CRT_memmove(void* dst, const void* src, size_t n);
    void* __cdecl CRT_malloc_zero(size_t size);
    void  __cdecl CRT_free(void* ptr);
    int   __cdecl CRT_rand(void);

}

/* Forward declarations for original game functions */
void CGWND_InitAllSubsystems(CGWND* self);
void CGWND_Cleanup(CGWND* self);

/* =========================================================================
 * main — boots the original game with SDL3 backend
 * ========================================================================= */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    printf("LEGO LOCO (SDL3 port)\n");
    printf("Booting original loco.exe game code...\n");

    /* ---- Phase 1: SDL3 init ---- */
    if (SDL3_Backend_Init("LEGO LOCO", 640, 480, 0) != 0) {
        fprintf(stderr, "FATAL: %s\n", SDL3_Backend_GetError());
        return 1;
    }

    /* ---- Phase 2: DirectDraw bridge ---- */
    SDL3_DDraw_Init();
    if (!g_ddraw || !g_backbuffer) {
        fprintf(stderr, "FATAL: DDraw bridge init failed\n");
        SDL3_Backend_Shutdown();
        return 1;
    }

    /* ---- Phase 3: DirectSound bridge ---- */
    g_pDS = LocoDS_Create(16, 22050);

    /* ---- Phase 4: Construct the original game's main object ---- */
    /* CGWND constructor expects HINSTANCE — we pass NULL */
    CGWND game(NULL);

    /* ---- Phase 5: Initialize all subsystems ---- */
    /* This calls into the ORIGINAL decompiled code! */
    printf("Calling CGWND::InitAllSubsystems...\n");
    game.InitAllSubsystems();

    /* The game loop is inside InitAllSubsystems / InitMode1 */
    /* It uses DirectDraw for rendering, DirectSound for audio */
    /* All of which are now backed by SDL3! */

    /* ---- Cleanup ---- */
    printf("Game exited. Shutting down...\n");
    game.Cleanup();

    SDL3_DDraw_Shutdown();
    if (g_pDS) { LocoDS_Destroy(g_pDS); g_pDS = NULL; }
    SDL3_Backend_Shutdown();

    printf("Goodbye.\n");
    return 0;
}
