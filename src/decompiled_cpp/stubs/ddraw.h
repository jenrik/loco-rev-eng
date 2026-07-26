/**
 * stubs/ddraw.h — Minimal DirectDraw type stubs
 *
 * Provides enough type definitions for the decompiled C++ code
 * to compile. No implementation — linking to ddraw.lib would be
 * needed for a working binary.
 */

#ifndef STUBS_DDRAW_H
#define STUBS_DDRAW_H

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* IDirectDraw4 / IDirectDrawSurface4 — versioned COM stubs           */
/* (outside extern "C" because they have C++ member functions)        */
/*                                                                     */
/* When LOCO_SDL3 is defined, forward-declare IDirectDraw4/           */
/* IDirectDrawSurface4 as typedefs so the bridge header can provide   */
/* the actual definitions.  When not LOCO_SDL3, provide stub methods. */
/* ================================================================== */

#ifdef LOCO_SDL3
  /* SDL3 mode: types are provided by port/sdl3_ddraw_bridge.h.
   * We forward-declare so that code using IDirectDraw4* still compiles. */
  #include "../port/sdl3_compat.h"
  /* sdl3_ddraw_bridge.h typedefs IDirectDraw4 → LocoDD,
   * IDirectDrawSurface4 → LocoSurface */
#else
  /* Stub mode (MinGW or native): empty inline methods */
  typedef struct IDirectDraw4 {
      void* vtable;
      int Release() { return 0; }
      int CreateSurface(void* a, void* b, void** c, void* d) { return 0; }
      int SetCooperativeLevel(void* a, int b) { return 0; }
      int SetDisplayMode(int a, int b, int c, int d, int e) { return 0; }
      int GetDeviceIdentifier(void* a, int b) { return 0; }
  } IDirectDraw4;

  typedef struct IDirectDrawSurface4 {
      void* vtable;
      int Release() { return 0; }
      int Blt(void* a, void* b, void* c, int d, void* e) { return 0; }
      int Lock(void* a, void* b, int c, int d) { return 0; }
      int Unlock(void* a) { return 0; }
      int GetDC(void** a) { return 0; }
      int ReleaseDC(void* a) { return 0; }
      int SetPalette(void* a) { return 0; }
      int GetSurfaceDesc(void* a) { return 0; }
      int BltFast(int a, int b, void* c, void* d, int e) { return 0; }
      int GetPixelFormat(void* a) { return 0; }
      int SetColorKey(int a, void* b) { return 0; }
      int IsLost() { return 0; }
      int Restore() { return 0; }
  } IDirectDrawSurface4;

  /* DDBLTFX stub */
  typedef struct _DDBLTFX {
      DWORD dwSize;
      DWORD dwDDFX;
  } DDBLTFX;

#endif /* LOCO_SDL3 */

#endif /* STUBS_DDRAW_H */#endif /* STUBS_DDRAW_H */
