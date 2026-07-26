/**
 * stubs/dsound.h — Minimal DirectSound type stubs
 */

#ifndef STUBS_DSOUND_H
#define STUBS_DSOUND_H

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* DirectSound interface stubs                                         */
/* ================================================================== */

typedef struct IDirectSound {
    void* vtable;
} IDirectSound;

typedef struct IDirectSoundBuffer {
    void* vtable;
} IDirectSoundBuffer;

/* ================================================================== */
/* Wave format                                                        */
/* ================================================================== */

typedef struct tWAVEFORMATEX {
    WORD   wFormatTag;
    WORD   nChannels;
    DWORD  nSamplesPerSec;
    DWORD  nAvgBytesPerSec;
    WORD   nBlockAlign;
    WORD   wBitsPerSample;
    WORD   cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX;

#define WAVE_FORMAT_PCM  1

/* ================================================================== */
/* DirectSound buffer description                                      */
/* ================================================================== */

typedef struct _DSBUFFERDESC {
    DWORD  dwSize;
    DWORD  dwFlags;
    DWORD  dwBufferBytes;
    DWORD  dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
} DSBUFFERDESC, *LPDSBUFFERDESC;

/* ================================================================== */
/* DSBCAPS                                                            */
/* ================================================================== */

typedef struct _DSBCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwUnlockTransferRate;
    DWORD dwPlayCpuOverhead;
} DSBCAPS;

/* ================================================================== */
/* DirectSound constants                                               */
/* ================================================================== */

#define DSSCL_NORMAL            1
#define DSSCL_PRIORITY          2
#define DSSCL_EXCLUSIVE         3
#define DSSCL_WRITEPRIMARY      4

#define DSBCAPS_PRIMARYBUFFER   0x00000001
#define DSBCAPS_STATIC          0x00000002
#define DSBCAPS_LOCHARDWARE     0x00000004
#define DSBCAPS_LOCSOFTWARE     0x00000008
#define DSBCAPS_CTRL3D          0x00000010
#define DSBCAPS_CTRLFREQUENCY   0x00000020
#define DSBCAPS_CTRLPAN         0x00000040
#define DSBCAPS_CTRLVOLUME      0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY 0x00000100
#define DSBCAPS_CTRLFX          0x00000200
#define DSBCAPS_GLOBALFOCUS     0x00008000

#define DSBPLAY_LOOPING         0x00000001

#define DSBVOLUME_MIN           (-10000)
#define DSBVOLUME_MAX           0

/* ================================================================== */
/* Error codes                                                         */
/* ================================================================== */

#define DS_OK                   0
#define DSERR_ALLOCATED         0x88780005
#define DSERR_CONTROLUNAVAIL    0x88780023
#define DSERR_INVALIDPARAM      0x88780032
#define DSERR_INVALIDCALL       0x88780064
#define DSERR_GENERIC           0x88780096
#define DSERR_PRIOLEVELNEEDED   0x887800C8
#define DSERR_OUTOFMEMORY       0x887800FA
#define DSERR_BADFORMAT         0x8878012C
#define DSERR_UNSUPPORTED       0x8878015E
#define DSERR_NODRIVER          0x88780190
#define DSERR_ALREADYINITIALIZED 0x887801F4
#define DSERR_NOAGGREGATION     0x88780226
#define DSERR_BUFFERLOST        0x88780296
#define DSERR_OTHERAPPHASPRIO   0x88780320
#define DSERR_UNINITIALIZED     0x88780352

#ifdef __cplusplus
}
#endif

#endif /* STUBS_DSOUND_H */
