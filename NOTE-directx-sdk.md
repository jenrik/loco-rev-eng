# DirectX SDK version used by Lego Loco

**Determination: DirectX 6.0 SDK (August 7, 1998)**

## Binary evidence (loco.exe)

### PE timestamp

Build date: **1998-10-06 16:12:45 UTC**

DirectX 6.0 was released August 7, 1998; DirectX 6.1 followed on February 3, 1999.
The ~2 month gap from DX6.0 release to this build rules out DX6.1.

### DLL imports

| DLL       | Import method          | Significance |
|-----------|------------------------|--------------|
| DDRAW.dll | DirectDrawCreate (name)| Classic DX6-era creation entry point |
| DPLAYX.dll| Ordinals 1, 4          | DPLAYX.dll introduced in DX6.0; DX5 shipped dplay.dll |
| DSOUND.dll| Ordinals 1, 2          | Ordinal binding common in DX5-DX6 era |

No DINPUT.dll import — the game does not use DirectInput.

### Interface GUID — IID_IDirectDraw4

At address 0x4785E8 in .rdata:

    9C59509A-39BD-11D1-8C4A-00C04FD930C5

This is the GUID for the IDirectDraw4 interface, introduced in **DirectX 6.0**.
It did not exist in DX5 or earlier.

The game obtains IDirectDraw4 via the classic two-step pattern visible in
DDRAW_GetSurface (0x45B500):

1. DirectDrawCreate(NULL, &dd, NULL) — gets bare IDirectDraw
2. dd->QueryInterface(IID_IDirectDraw4, &g_ddraw) — upgrades to IDirectDraw4
3. Stores result in global g_ddraw (0x485440)

This pattern (rather than using DirectDrawCreateEx) and targeting IDirectDraw4
(rather than IDirectDraw2 or IDirectDraw7) locks the SDK to the DX6 window.

### Full GUID table in .rdata (0x4785C8 region)

| Address   | GUID                                   | Identity                   |
|-----------|----------------------------------------|----------------------------|
| 0x4785C8  | 6C14DB80-A733-11CE-A521-0020AF0BE560   | IID_IDirectDraw            |
| 0x4785D8  | B3A6F3E0-2B43-11CF-A2DE-00AA00B93356   | IID_IDirectDraw2           |
| 0x4785E8  | 9C59509A-39BD-11D1-8C4A-00C04FD930C5   | **IID_IDirectDraw4** (used)|
| 0x4785F8  | 6C14DB81-A733-11CE-A521-0020AF0BE560   | IID_IDirectDrawSurface     |
| 0x478608  | 57805885-6EEC-11CF-9441-A823-03C10E27   | surface-related (TBD)      |
| 0x478618  | DA044E00-69B2-11D0-A1D5-00AA00B8DFBB   | D3D-related                |

### D3DRM error codes

DDRAW_GetDdrawErrorString (0x45BBC0) — the game's error-string lookup function —
handles three error-code families:

- DDERR_* (MAKE_DDHRESULT range) — standard DirectDraw errors
- D3DERR_* — Direct3D Immediate Mode execute-buffer errors
- D3DRMERR_* — **Direct3D Retained Mode** errors

D3DRM was part of the core DirectX SDK through version 6. It was removed from
the core starting with DirectX 7.0 (September 1999).

### DirectPlay error strings

The game contains format strings referencing "Direct Play" (note the space,
older naming convention) and DPERR_CANTADDPLAYER / DPERR_INVALIDPLAYER —
error codes defined in dplay.h from the DX6 era.

## Sources for the SDK

### Primary: Internet Archive

- **dx-6-sdk** — https://archive.org/details/dx-6-sdk
  - ISO: Dx6SDK.iso (~430 MB)
- **directx6sdk** — https://archive.org/details/directx6sdk
  - Alternate upload of the same SDK

The ISO contains the exact headers (ddraw.h, dplay.h, dsound.h, d3drm.h, etc.)
and import libraries (ddraw.lib, dplayx.lib, dsound.lib) the game was built against.

### Secondary: Olde-Skuul GitHub

- https://github.com/Olde-Skuul/directdraw — ddraw.h, ddraw.lib, samples
- https://github.com/Olde-Skuul/directplay — dplay.h, dplayx.lib, dpaddr.h

These are modern reconstructions for contemporary toolchains. They retain
API compatibility for the interfaces used by loco.exe but are not the
original DX6.0 SDK bits. Useful for quick header reference.

### For reference (do not use)

- ms-dx61-sdk on archive.org — DirectX 6.1 (February 1999), too new
- dx-5-sdk and similar — too old; missing IID_IDirectDraw4 and DPLAYX.dll
- NovaRain/DXSDK_Collection (GitHub) — starts at Aug 2007, DX9 era only
- apitrace/dxsdk (GitHub) — DX8+ headers only

## Relevant interface versions per subsystem

| Subsystem    | Interface used                    | Introduced in |
|--------------|-----------------------------------|---------------|
| DirectDraw   | IDirectDraw4                      | DirectX 6.0   |
| DirectSound  | IDirectSound (ordinal 1)          | DirectX 3 / 5 |
| DirectPlay   | IDirectPlay4A (via DPLAYX.dll)    | DirectX 6.0   |
| D3DRM        | (error codes only, no create)     | DX2–DX6       |

No Direct3D Immediate Mode or DirectInput usage detected.