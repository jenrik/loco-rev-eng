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

/* ================================================================== */
/* IDirectDraw4 / IDirectDrawSurface4 — versioned COM stubs           */
/* (outside extern "C" because they have C++ member functions)        */
/*                                                                     */
/* When LOCO_SDL3 is defined, forward-declare IDirectDraw4/           */
/* IDirectDrawSurface4 as typedefs so the bridge header can provide   */
/* the actual definitions.  When not LOCO_SDL3, provide stub methods. */
/* ================================================================== */

  /* Stub mode: empty inline methods. The SDL3 host provides real
   * implementations via src/sdl3_shims/sdl3_ddraw.h. */
  typedef struct IDirectDraw4 {
      void* vtable;
      int Release() { return 0; }
      int CreateSurface(void* desc, void** out, void* unused) { return 0; }
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

#endif /* STUBS_DDRAW_H */
