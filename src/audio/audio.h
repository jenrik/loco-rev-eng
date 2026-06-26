/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Audio / DirectSound / MCIWnd
 * Original: loco.exe (Windows 95/98, DirectX 5 era)
 * Developer: Intelligent Games for LEGO Media
 *
 * This file was produced by reverse engineering the original binary.
 * Windows API calls are marked with WIN32: comments.
 * Linux/SDL2 replacement suggestions are marked with LINUX: comments.
 */

#ifndef NETWORK_H
#define NETWORK_H

/* WIN32 */
#include <windows.h>
/* LINUX:
 * #include <stdint.h>
 * #include <stddef.h>
 * typedef uint8_t  BYTE;
 * typedef uint16_t WORD;
 * typedef uint32_t DWORD;
 * typedef int32_t  LONG;
 * typedef char *   LPSTR;
 * typedef const char * LPCSTR;
 * typedef void *   HWND;
 * typedef void *   HINSTANCE;
 * typedef void *   HMODULE;
 * typedef void *   HBRUSH;
 * typedef void *   HANDLE;
 * typedef DWORD    MCIDEVICEID;
 * typedef intptr_t LPARAM;
 * typedef uintptr_t DWORD_PTR;
 * #define INVALID_HANDLE_VALUE ((HANDLE)-1)
 * #define GENERIC_READ  0x80000000u
 * #define GENERIC_WRITE 0x40000000u
 * #define FILE_SHARE_READ    0x00000001
 * #define OPEN_EXISTING      3
 * #define CREATE_ALWAYS      2
 * #define FILE_FLAG_NO_BUFFERING 0x20000000
 * #define SW_HIDE 0
 * #define GWL_WNDPROC (-4)
 */

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Structs / Types
 * ========================================================================= */

/*
 * CIniFile -- Thin wrapper around Windows INI-file APIs.
 *
 * Singleton lives at DAT_004a9eec.
 * Offset +4 holds the null-terminated path to the INI file.
 * Vtable set to PTR_LAB_004784bc by CIniFile_Construct (FUN_00452d50).
 *
 * INI sections used by the audio/profile subsystem:
 *   [MOUSE]       -- username (Setting1/2/3)
 *   [CLIENT]      -- NextId = next user slot number (1..999)
 *   [ScreenSaver] -- Sound = 0 means music plays during screensaver
 *   [Sound]       -- reserved for future audio settings
 *
 * LINUX replacement: inih (https://github.com/benhoyt/inih) with
 *   ini_parse() / ini_get_int() / ini_get_string() / custom ini_write().
 *   Store config at ~/.config/lego-loco/loco.ini (XDG_CONFIG_HOME).
 */
typedef struct CIniFile {
    void  *vtable;       /* 0x00: PTR_LAB_004784bc                         */
    char   iniFilePath[1]; /* 0x04: variable-length path string             */
} CIniFile;

/*
 * CUserProfile -- One player's identity and save-game progress.
 *
 * Serialised to a 0x120-byte .usr binary file under the save directory.
 * Path format: "%s%s.usr" (DAT_004a99c8 base dir + username stem).
 * Constructor: CUserProfile_Construct (FUN_00452e10).
 * Total object size: ~0x124 bytes (including isNewUser flag at +0x120).
 *
 * LINUX replacement for file I/O: POSIX open/read/write/close.
 * LINUX replacement for GetUserNameA: getpwuid(getuid())->pw_name.
 */
typedef struct CUserProfile {
    void     *vtable;           /* 0x00: PTR_LAB_004784c0                  */
    uint16_t  magic;            /* 0x04: file format version (0x0066)       */
    char      username[12];     /* 0x06: player name (null-terminated)      */
    uint8_t   _pad0[6];         /* 0x12: padding to align to 0x14           */
    uint32_t  reserved;         /* 0x14: reserved field                     */
    uint32_t  clientId;         /* 0x18: slot number (1..999)               */
    uint32_t  saveCounter;      /* 0x1c: screenshot counter (0..9999, wraps)*/
    char      lastScreenshotName[32]; /* 0x20: formatted as "%03d_%04d"     */
    /* ... remainder of 0x120-byte save block ... */
    uint8_t   isNewUser;        /* 0x120: 1 = brand-new account             */
} CUserProfile;

/*
 * CMciVideoPlayer -- AVI/WAV playback via the MCIWnd window class.
 *
 * Used for intro videos (IgSpin.avi, legoSpin.avi) and background music
 * (music.wav) via MSVFW32.DLL MCIWndRegisterClass / CreateWindowExA.
 * Constructor: CMciVideoPlayer_Construct (FUN_00454250).
 * Destructor:  CMciVideoPlayer_Destruct  (FUN_00454330).
 * Size: ~0x28 bytes (plus variable mediaFilePath string at +0x20).
 *
 * MCIWnd messages:
 *   0x499 = MCIWNDM_OPEN         -- open media file
 *   0x48F = MCIWNDM_PUT_DEST     -- set display rect
 *   0x464 = MCIWNDM_GETDEVICEID  -- get MCI device ID
 *   0x804 = MCIWNDM_STOP         -- stop playback
 *   0x010 = WM_CLOSE             -- close / destroy MCIWnd
 *
 * LINUX replacement: GStreamer (gst_parse_launch "playbin uri=...") or
 *   libVLC (libvlc_media_player_set_xwindow + libvlc_media_player_play).
 */
typedef struct CMciVideoPlayer {
    void      *vtable;          /* 0x00: PTR_FUN_004784c4                   */
    HWND       hMciWnd;         /* 0x04: MCIWnd child window (NULL = idle)  */
    int        rect[4];         /* 0x08: left, top, right, bottom           */
    HWND       hParentWnd;      /* 0x18: parent window handle               */
    HINSTANCE  hInstance;       /* 0x1c: application HINSTANCE              */
    char       mediaFilePath[1];/* 0x20: null-terminated media file path    */
} CMciVideoPlayer;

/*
 * CGameAudioManager -- Top-level audio/game-state controller.
 *
 * Global singleton at DAT_004aa4a8 (0x124 = 292 bytes allocated).
 * Presence is checked before all sound helpers (FUN_0041a0e0, FUN_00445170).
 * Holds references to the video player, DirectSound SFX system, and
 * music playback state machine.
 *
 * Audio state machine in CAudioStateMachine_SetState (FUN_004208f0):
 *   State 0 = initial / idle
 *   State 1 = music stopped (transitioning to menu)
 *   State 2 = paused / overlay hidden
 *   State 3 = video playing (MusicController -> Play)
 *   State 4 = video fullscreen, no audio overlay
 *   State 5 = video with audio track
 *   State 6 = return to main menu
 *   State 7 = in-game: play music.wav via PlaySoundA, stop video player
 */
typedef struct CGameAudioManager {
    void           *vtable;           /* 0x000                               */
    uint8_t         _pad0[0xe4];      /* 0x004: padding to 0xe8              */
    int32_t         currentAudioState;/* 0xe8: see state values above        */
    uint8_t         _pad1[0x11c];     /* 0xec: padding to 0x20c              */
    HWND            hVideoWindow;     /* 0x20c: video overlay window         */
    CMciVideoPlayer *pVideoPlayer;    /* 0x210: active video player or NULL  */
    uint8_t         _pad2[4];         /* 0x214                               */
    LONG            savedWindowLong;  /* 0x218: saved GWL_WNDPROC value      */
    void          **ppMusicController;/* 0x21c: vtable ptr to music ctrl     */
    void          **ppSfxController;  /* 0x220: vtable ptr to SFX ctrl       */
} CGameAudioManager;

/*
 * CSoundEffect -- Wraps a single DirectSound buffer (IDirectSoundBuffer).
 *
 * Obtained via DSOUND.DLL Ordinal_1 (DirectSoundCreate) through the chain:
 *   CSoundEffect_Construct (FUN_0044be50) -> FUN_0040d500 -> DirectSoundCreate.
 * Stores 3D positional parameters, loop/oneshot flag, volume scalars, and a
 * pointer back to the owning game object.
 * Constructor: CSoundEffect_Construct (FUN_0044be50).  Size: ~0x94 bytes.
 *
 * LINUX replacement:
 *   OpenAL: alGenBuffers + alBufferData + alGenSources + alSourcePlay.
 *   SDL2_mixer: Mix_LoadWAV + Mix_PlayChannel.
 */
typedef struct CSoundEffect {
    void     *vtable;          /* 0x00: PTR_FUN_0047836c                     */
    uint32_t  sfxFlags;        /* 0x04: control flags                        */
    int32_t   bufferOffset;    /* 0x08                                       */
    uint16_t  bufferIndex;     /* 0x0c                                       */
    uint8_t   _pad0[2];        /* 0x0e                                       */
    void     *pDSBuffer;       /* 0x10: IDirectSoundBuffer* (or array start) */
    uint8_t   _pad1[0x0c];     /* 0x14                                       */
    void     *pPositionData;   /* 0x20: 3D position data block (0x20 bytes)  */
    int16_t   panLeft;         /* 0x24                                       */
    int16_t   panRight;        /* 0x26                                       */
    uint32_t  soundId;         /* 0x28                                       */
    uint8_t   _loopCtrl;       /* 0x2c                                       */
    uint8_t   _pad2[1];        /* 0x2d                                       */
    uint16_t  replayTimer0;    /* 0x2e                                       */
    uint16_t  replayTimer1;    /* 0x30                                       */
    uint16_t  replayTimer2;    /* 0x32                                       */
    uint16_t  replayTimer3;    /* 0x34                                       */
    uint16_t  loopCount;       /* 0x36                                       */
    uint8_t   _pad3[0x1e];     /* 0x38: eight uint32 state fields (zeroed)   */
    int16_t   basePan;         /* 0x58                                       */
    uint8_t   isLooping;       /* 0x5a                                       */
    uint8_t   _pad4[5];        /* 0x5b                                       */
    uint32_t  playState;       /* 0x60: 0=stopped, 2=playing                 */
    uint8_t   _pad5[0x04];     /* 0x64                                       */
    uint32_t  dsBufferState;   /* 0x68                                       */
    uint8_t   _pad6[0x08];     /* 0x6c                                       */
    uint32_t  positionState;   /* 0x70                                       */
    uint8_t   _pad7[0x08];     /* 0x74                                       */
    uint8_t   volumeRain;      /* 0x78 (actually at runtime offset 0x78)     */
    uint8_t   _pad8[1];        /* 0x79                                       */
    int16_t   mixerSlot;       /* 0x7a                                       */
    uint8_t   volumeTrain;     /* 0x7c                                       */
    uint8_t   _pad9[0x0b];     /* 0x7d                                       */
    uint8_t   layerIndex;      /* 0x88                                       */
    uint8_t   sfxLayer0;       /* 0x89                                       */
    uint8_t   sfxLayer1;       /* 0x8a                                       */
    uint8_t   _pad10[5];       /* 0x8b                                       */
    uint8_t   attenFlag;       /* 0x90                                       */
} CSoundEffect;

/*
 * CSoundHandle -- Lightweight per-object sound ownership handle.
 *
 * Used by tile-grid objects to hold a reference to a playing CSoundEffect.
 * Constructor: CSoundHandle_Construct (FUN_00454b50).  Size: ~0x24 bytes.
 * Destructor:  CSoundHandle_Destruct  (FUN_00454b70).
 *
 * LINUX replacement:
 *   Replace pEffect with an OpenAL source ID (ALuint) or SDL2_mixer channel.
 */
typedef struct CSoundHandle {
    void     *vtable;          /* 0x00: PTR_FUN_0047851c                     */
    uint8_t   _pad0[0x10];     /* 0x04                                       */
    CSoundEffect *pEffect;     /* 0x14: pointer to active CSoundEffect       */
    int32_t   playPosition;    /* 0x18: current play position                */
    uint32_t  soundResourceId; /* 0x1c: resource ID for CSoundTable lookup   */
    uint32_t  reserved;        /* 0x20                                       */
} CSoundHandle;

/*
 * CLoopingSoundObject -- Ambient / looping sound (train, rain, etc.).
 *
 * Extends the base one-shot sound object (FUN_0040cfa0).
 * sfxType field is set to 8 (looping ambient).
 * Constructor: CLoopingSoundObject_Construct (FUN_00448f30). Size: ~0x68 bytes.
 *
 * LINUX replacement:
 *   Mix_PlayChannel(-1, chunk, -1) for infinite loop (SDL2_mixer).
 *   OR alSourcei(alSrc, AL_LOOPING, AL_TRUE) + alSourcePlay(alSrc) (OpenAL).
 */
typedef struct CLoopingSoundObject {
    void     *vtable;          /* 0x00: PTR_FUN_00478280                     */
    uint32_t  sfxType;         /* 0x04: 8 = looping ambient                 */
    uint8_t   _pad0[0x24];     /* 0x08                                       */
    uint32_t  loopFlags;       /* 0x28                                       */
    uint16_t  loopParam;       /* 0x2c                                       */
    uint8_t   _pad1[0x1e];     /* 0x2e                                       */
    int32_t   posX;            /* 0x4c                                       */
    int32_t   posY;            /* 0x50                                       */
    uint16_t  panValue;        /* 0x54                                       */
    uint8_t   activeFlag;      /* 0x56                                       */
    uint8_t   eeReplayFlag;    /* 0x58                                       */
    uint8_t   _pad2[3];        /* 0x59                                       */
    int32_t   stringLength;    /* 0x5c: allocated length of pFilename buffer */
    char     *pFilename;       /* 0x60: pointer to filename string           */
    uint32_t  resourceReplayDelay; /* 0x64                                   */
} CLoopingSoundObject;

/* =========================================================================
 * Function Declarations
 * ========================================================================= */

/* --- CIniFile (FUN_00452d50 .. FUN_00452df0) --- */

/*
 * CIniFile_Construct -- Set vtable pointer of a freshly-allocated CIniFile.
 * No Windows APIs called directly.
 * WIN32: (none)  LINUX: (none)
 */
void __fastcall CIniFile_Construct(void **pObj);

/*
 * CIniFile_ReadInt -- Read an integer from [section] key in the INI file.
 * Returns defaultVal if the key is absent.
 * WIN32: GetPrivateProfileIntA
 * LINUX: inih ini_get_int(cfg, section, key, defaultVal)
 */
int __thiscall CIniFile_ReadInt(void  *this,
                                LPCSTR section,
                                LPCSTR key,
                                INT    defaultVal);

/*
 * CIniFile_ReadString -- Read a string from [section] key in the INI file.
 * Used to retrieve the stored player username from the [MOUSE] section.
 * WIN32: GetPrivateProfileStringA
 * LINUX: inih ini_get_string(cfg, section, key, defaultVal, outBuf, bufSize)
 */
void __thiscall CIniFile_ReadString(void  *this,
                                    LPCSTR section,
                                    LPCSTR key,
                                    LPCSTR defaultVal,
                                    LPSTR  outBuf,
                                    DWORD  bufSize);

/*
 * CIniFile_WriteInt -- Convert value to decimal string and write to INI.
 * Used to persist the incremented CLIENT/NextId counter.
 * WIN32: WritePrivateProfileStringA
 * LINUX: custom ini_set_int() + fflush
 */
void __thiscall CIniFile_WriteInt(void  *this,
                                  LPCSTR section,
                                  LPCSTR key,
                                  uint   value);

/*
 * CIniFile_WriteString -- Write a raw string to [section] key in the INI file.
 * WIN32: WritePrivateProfileStringA
 * LINUX: custom ini_set_string() + fflush
 */
void __thiscall CIniFile_WriteString(void  *this,
                                     LPCSTR section,
                                     LPCSTR key,
                                     LPCSTR value);

/* --- CUserProfile (FUN_00452e10 .. FUN_00453320) --- */

/*
 * CUserProfile_Construct -- Initialise a user-profile object.
 * Reads username from INI [MOUSE]; falls back to GetUserNameA then "LEGO LOCO".
 * Loads or creates the .usr save file. Sets isNewUser flag.
 * Triggers audio/UI refresh if the global audio manager is live.
 * WIN32: GetUserNameA, GetPrivateProfileStringA (via CIniFile_ReadString)
 * LINUX: getpwuid(getuid())->pw_name for username fallback
 */
void *__fastcall CUserProfile_Construct(uint32_t *pProfile);

/*
 * CUserProfile_SetUsername -- Update the stored username and reload .usr file.
 * Mirrors logic in CUserProfile_Construct; no direct Windows API calls.
 */
void __thiscall CUserProfile_SetUsername(void *this, uint8_t *pNewUsername);

/*
 * CUserProfile_LoadFromFile -- Read the 0x120-byte .usr binary from disk.
 * Builds path "%s%s.usr", opens read-only. Validates magic == 0x66.
 * On bad magic: reassigns slot from CLIENT/NextId and rewrites the file.
 * Returns 1 on success, 0 on file-not-found or read error.
 * WIN32: wsprintfA, CreateFileA, ReadFile, WriteFile, CloseHandle
 * LINUX: snprintf + open/read/write/close (POSIX unistd.h)
 */
int __fastcall CUserProfile_LoadFromFile(int pProfile);

/*
 * CUserProfile_SaveToFile -- Write the 0x120-byte profile block to disk.
 * WIN32: wsprintfA, CreateFileA, WriteFile, CloseHandle
 * LINUX: snprintf + open(O_WRONLY|O_CREAT|O_TRUNC, 0644) + write + close
 */
void __fastcall CUserProfile_SaveToFile(int pProfile);

/*
 * CUserProfile_MakeScreenshotName -- Format next screenshot filename.
 * Writes "%03d_%04d" (clientId, saveCounter) to pProfile+0x20.
 * Increments and wraps saveCounter at 9999. Flushes profile to disk.
 * Returns pointer to the name buffer (pProfile+0x20).
 * WIN32: wsprintfA, CreateFileA, WriteFile, CloseHandle
 * LINUX: snprintf + POSIX file I/O
 */
char *__fastcall CUserProfile_MakeScreenshotName(int pProfile);

/* --- CMciVideoPlayer (FUN_00454250 .. FUN_004544a0) --- */

/*
 * CMciVideoPlayer_Construct -- Create a video player and start playback.
 * Verifies file exists via CreateFileA; gets client rect via GetClientRect.
 * Posts WM_USER+0x3b9 to parent if file not found.
 * WIN32: CreateFileA, CloseHandle, GetClientRect, PostMessageA
 * LINUX: access(path, F_OK); SDL_GetWindowSize; GStreamer or libVLC
 */
void *__thiscall CMciVideoPlayer_Construct(void      *this,
                                           HWND       hParent,
                                           HINSTANCE  hInstance,
                                           char      *pFilePath);

/*
 * CMciVideoPlayer_Destruct -- Stop MCIWnd and optionally free memory.
 * Sends MCIWNDM_STOP (0x804) + WM_CLOSE (0x10) to hMciWnd.
 * WIN32: SendMessageA
 * LINUX: gst_element_set_state(GST_STATE_NULL) + gst_object_unref
 */
void *__thiscall CMciVideoPlayer_Destruct(void *this, BYTE freeMemory);

/*
 * CMciVideoPlayer_PlayInWindow -- Core video/audio playback routine.
 * Stops existing MCIWnd; registers class; creates child window;
 * opens media file (MCIWNDM_OPEN 0x499); sizes it (MCIWNDM_PUT_DEST 0x48F);
 * gets device ID (MCIWNDM_GETDEVICEID 0x464); sets volume via mciSendCommandA.
 * WIN32: SendMessageA, MCIWndRegisterClass, CreateWindowExA, mciSendCommandA
 * LINUX: GStreamer playbin pipeline or libVLC embedded in SDL window
 */
void __thiscall CMciVideoPlayer_PlayInWindow(void   *this,
                                             LPARAM  pFilePath,
                                             int    *pRect);

/*
 * CMciVideoPlayer_Stop -- Non-destructive stop: send STOP + CLOSE, null handle.
 * WIN32: SendMessageA (0x804=MCIWNDM_STOP, 0x10=WM_CLOSE)
 * LINUX: gst_element_set_state(pipeline, GST_STATE_NULL)
 */
void __fastcall CMciVideoPlayer_Stop(int pPlayer);

/* --- CAudioStateMachine (FUN_004208f0) --- */

/*
 * CAudioStateMachine_SetState -- Central audio/game-mode state machine.
 * State 1: PlaySoundA(NULL) to stop music.
 * State 7: wsprintfA + PlaySoundA(SND_FILENAME|SND_ASYNC) for music.wav.
 * State 3: vtable[1] (Play) on MusicController.
 * State 6: stop both controllers, switch to main menu.
 * WIN32: ShowWindow, SetWindowLongA, PlaySoundA, wsprintfA
 * LINUX: Mix_HaltMusic / Mix_PlayMusic (SDL2_mixer); GStreamer for video
 */
void __thiscall CAudioStateMachine_SetState(void *this, int newState);

/* --- Screensaver / global game sound (FUN_004480c0, FUN_0045e090) --- */

/*
 * CScreenSaver_PlayMusic -- Play music.wav when screensaver activates.
 * Reads [ScreenSaver] Sound INI key; plays if value == 0 (enabled).
 * WIN32: GetPrivateProfileIntA (via CIniFile_ReadInt), wsprintfA, PlaySoundA
 * LINUX: custom INI read; Mix_LoadMUS + Mix_PlayMusic
 */
int __fastcall CScreenSaver_PlayMusic(int param_1);

/*
 * CGame_StopAllSounds -- Immediately stop all music; clear screen to black.
 * WIN32: PlaySoundA, GetStockObject, FillRect, GetSystemMetrics
 * LINUX: Mix_HaltMusic; SDL_SetRenderDrawColor + SDL_RenderClear
 */
void CGame_StopAllSounds(void);

/* --- CSoundTable (FUN_00446ea0) --- */

/*
 * CSoundTable_LoadSoundById -- Look up a sound by ID in the resource table.
 * If unloaded, calls GetModuleHandleA + LoadStringA to get the resource name.
 * Language variants (game modes 1-9) add fixed offsets to soundId.
 * WIN32: GetModuleHandleA, LoadStringA
 * LINUX: custom resource bundle lookup (e.g. sounds_table.txt)
 */
int __thiscall CSoundTable_LoadSoundById(void *this, int soundId);

/* --- CSoundEffect (FUN_0044be50) --- */

/*
 * CSoundEffect_Construct -- Initialise a DirectSound buffer wrapper.
 * Sets replay timers, allocates 3D position block and DS buffer (0x450 bytes).
 * Calls FUN_0040d500 -> DirectSoundCreate (DSOUND.DLL Ordinal_1).
 * Starts one-shot or looping playback via FUN_0044d500.
 * WIN32 (indirect): DirectSoundCreate, IDirectSoundBuffer::Play
 * LINUX: alGenBuffers + alBufferData + alGenSources + alSourcePlay (OpenAL)
 *     OR Mix_LoadWAV + Mix_PlayChannel (SDL2_mixer)
 */
void *__thiscall CSoundEffect_Construct(void     *this,
                                        int       soundId,
                                        uint32_t  flags,
                                        char      isLooping,
                                        uint8_t   layerIndex);

/*
 * CGameObject_HasActiveSound -- Return 1 if the object is emitting sound.
 * Checks state byte at pObj+0x63a (1=queued, 2=playing, 3=looping, 4=finishing).
 * LINUX: alGetSourcei(alSrc, AL_SOURCE_STATE) == AL_PLAYING
 */
int __fastcall CGameObject_HasActiveSound(int pObj);

/* --- CLoopingSoundObject (FUN_00448f30) --- */

/*
 * CLoopingSoundObject_Construct -- Create an ambient/looping sound object.
 * Calls FUN_0040cfa0 (base one-shot ctor), overrides vtable, sets sfxType=8.
 * Allocates filename buffer of fileNameLen+1 bytes; stores replayDelay at +100.
 * LINUX: Mix_PlayChannel(-1, chunk, -1) or alSourcei(AL_LOOPING, AL_TRUE)
 */
void *__thiscall CLoopingSoundObject_Construct(void     *this,
                                               int       fileNameLen,
                                               int       ownerPtr,
                                               int       mapPtr,
                                               uint32_t  replayDelay,
                                               uint16_t  panValue);

/* --- CTileObject (FUN_004546d0) --- */

/*
 * CTileObject_AttachSound -- Attach a sound effect to a tile object.
 * loopSource==0: allocate 0x58-byte one-shot via FUN_0040cfa0.
 * loopSource!=0: allocate 0x68-byte looping via FUN_00448f30.
 * Updates global sound-slot list (DAT_00485270).
 * WIN32 (indirect): DirectSoundCreate chain
 * LINUX: Mix_LoadWAV + Mix_PlayChannel OR alGenBuffers + alSourcePlay
 */
void *__thiscall CTileObject_AttachSound(void    *this,
                                         int      soundId,
                                         uint16_t panValue,
                                         int      loopSource);

/* --- CSoundHandle (FUN_00454b50 .. FUN_00454bf0) --- */

/*
 * CSoundHandle_Construct -- Minimal constructor: set vtable, store resource ID.
 * Zeros pEffect and playPosition.
 */
void __thiscall CSoundHandle_Construct(void *this, uint32_t soundResourceId);

/*
 * CSoundHandle_Destruct -- Full destructor: stop sound via vtable[2], free.
 * WIN32 (indirect): IDirectSoundBuffer::Stop
 * LINUX: alSourceStop + alDeleteSources + alDeleteBuffers (OpenAL)
 */
void *__thiscall CSoundHandle_Destruct(void *this, BYTE freeMemory);

/*
 * CSoundHandle_Stop -- Non-destructive stop: call vtable[2] on pEffect.
 * WIN32 (indirect): IDirectSoundBuffer::Stop
 * LINUX: alSourceStop / Mix_HaltChannel
 */
void __fastcall CSoundHandle_Stop(int pHandle);

/*
 * CSoundHandle_LoadAndPlay -- Resolve sound resource and start playback.
 * Calls CSoundTable_LoadSoundById then vtable[1] (GetBuffer/Play) on result.
 * Returns 1 if a valid buffer was obtained, 0 otherwise.
 * WIN32 (indirect): DirectSoundCreate, IDirectSoundBuffer::Play
 * LINUX: alSourcePlay (OpenAL) or Mix_PlayChannel (SDL2_mixer)
 */
int __fastcall CSoundHandle_LoadAndPlay(int pHandle);

/* --- CGameWorld (FUN_00454cf0 .. FUN_00454e60, FUN_00454fe0) --- */

/*
 * CGameWorld_Construct -- Construct game world with two ambient sound channels.
 * Allocates rain/ambient (slot 7) and train/music (slot 8) sound objects.
 * Resets tile grid and clears all sound-handle pointers.
 * WIN32 (indirect): DirectSoundCreate chain
 * LINUX: alcCreateContext + alGenSources (OpenAL) or Mix_PlayChannel
 */
void *__fastcall CGameWorld_Construct(uint32_t *pWorld);

/*
 * CGameWorld_Cleanup -- Destroy ambient sound channels and visibility bitmap.
 * Calls FUN_0045ce10 (stop + release DS buffer) + FUN_00465cd0 (delete) on each.
 * WIN32 (indirect): IDirectSoundBuffer::Release
 * LINUX: alDeleteSources + alDeleteBuffers (OpenAL) or Mix_FreeChunk
 */
void __fastcall CGameWorld_Cleanup(int pWorld);

/*
 * CGameWorld_Reset -- Zero tile grid, reset visibility bitmap, repaint window.
 * Clears 0x14910 uint32 tile slots; fills visibility bitmap with 0xFF.
 * WIN32: InvalidateRect, UpdateWindow
 * LINUX: SDL_RenderPresent or dirty-region queue signal
 */
void __fastcall CGameWorld_Reset(int pWorld);

/*
 * CGameWorld_SetResolution -- Configure tile-grid dimensions from screen size.
 * Defaults to 1024x768 if screen width outside 0x400..0x500.
 * Reallocates per-tile visibility bitmap (1 bit per tile, rounded up).
 * LINUX: SDL_GetCurrentDisplayMode(0, &mode) for screen dimensions
 */
void __thiscall CGameWorld_SetResolution(void *this, char isHighRes);

/* --- Tile map operations (FUN_004550c0, FUN_004553e0, FUN_00455ab0) --- */

/*
 * CTileMap_PlaceItem -- Place a game object at tile (tileX, tileY).
 * Bounds-checks coordinates, looks up object type, fills tile slots,
 * updates layer-count bytes, marks tiles dirty, calls vtable[3] SetPosition.
 * LINUX: pure game logic
 */
int *__thiscall CTileMap_PlaceItem(void  *this,
                                   uint   soundId,
                                   short  tileX,
                                   short  tileY,
                                   char   checkOnly,
                                   uint   sourceSlot);

/*
 * CTileMap_CheckPlacement -- Validate or execute item placement on tile grid.
 * Checks footprint fits within grid. If doPlace!=0 and cell occupied,
 * calls CTileMap_RemoveItem with pick-up sound event.
 * Returns 1 if placement is valid/successful.
 * LINUX: pure game logic
 */
char __thiscall CTileMap_CheckPlacement(void  *this,
                                        char   doPlace,
                                        void  *pItemType,
                                        short  tileX,
                                        uint16_t tileY,
                                        uint32_t sourceSlot);

/*
 * CTileMap_RemoveItem -- Remove a game object from the tile grid.
 * Clears object pointer from each footprint cell's layer slots.
 * Calls FUN_0041def0 to remove from game-object list and trigger PickUpSoundId.
 * LINUX: no direct platform APIs; dirty-tile marking drives SDL repaint
 */
uint32_t __thiscall CTileMap_RemoveItem(void     *this,
                                        uint32_t *pItem,
                                        uint32_t  removeMode);

/*
 * CTileMap_DrawTrack -- Draw a straight track between two pixel coordinates.
 * Interpolates along the line, removes topmost tile objects at each step.
 * LINUX: pure game logic; FPU intrinsics are standard C math
 */
uint32_t __thiscall CTileMap_DrawTrack(void *this,
                                       int   x1,
                                       int   y1,
                                       int   x2,
                                       int   y2);

/*
 * CTileMap_HandleClick -- Main mouse-click/interaction handler.
 * Converts pixel to tile coords; branches on game phase.
 * Phase 3 = delete mode; phase 4 = place/paint mode.
 * Returns 1 if event consumed.
 * LINUX: pure game logic; sound IDs map to OpenAL/SDL2_mixer channels
 */
uint8_t __thiscall CTileMap_HandleClick(void *this, int pixelX, int pixelY);

/* --- Game object helpers --- */

/*
 * CGameObject_Construct -- Call base ctor FUN_00433a20, override vtable.
 * Sets sprite/audio layer field at this+0x88 to 4 (ambient sound layer).
 */
uint32_t *__thiscall CGameObject_Construct(void *this, int param_1);

/*
 * CGameObject_Destruct -- Destructor: reset vtable, check global active-object,
 * tear down audio subsystem via FUN_004113a0 if needed. Uses SEH cleanup.
 */
void __fastcall CGameObject_Destruct(uint32_t *pObj);

/*
 * CGameObject_CheckSoundTrigger -- Check if a sound should trigger at (gridX, gridY).
 * Reads sound-state byte at this+0x63a; compares against position offsets.
 * LINUX: pure game logic
 */
uint __thiscall CGameObject_CheckSoundTrigger(void *this,
                                              short  gridX,
                                              uint16_t gridY);

/*
 * CGame_LoadWorld -- World/level loading routine.
 * Sets periodic timer (id=0x47, 150ms) for animation.
 * WIN32: SetTimer, PlaySoundA, InvalidateRect, UpdateWindow
 * LINUX: SDL_AddTimer or timerfd_create; SDL_RenderPresent
 */
void CGame_LoadWorld(void);

/* =========================================================================
 * LINUX conditional compilation stubs
 *
 * Compile with -DLOCO_LINUX to enable the Linux backend stubs below.
 * These replace the Windows audio/INI/file APIs with POSIX / SDL2 / OpenAL.
 * ========================================================================= */

#ifdef LOCO_LINUX

/* SDL2_mixer replacements for PlaySoundA */
#include <SDL2/SDL_mixer.h>
/* Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) at startup            */
/* Mix_LoadMUS(path) + Mix_PlayMusic(music, -1) for looping background     */
/* Mix_HaltMusic()  replaces PlaySoundA(NULL, 0, 0)                        */

/* OpenAL replacements for DirectSoundCreate */
#include <AL/al.h>
#include <AL/alc.h>
/* alcOpenDevice(NULL) + alcCreateContext + alcMakeContextCurrent           */
/* alGenBuffers / alBufferData / alGenSources / alSourcePlay                */
/* alSourceStop + alDeleteSources + alDeleteBuffers for cleanup             */

/* GStreamer replacements for MCIWnd video */
#include <gst/gst.h>
/* gst_init(NULL, NULL); gst_parse_launch("playbin uri=file://...")         */
/* gst_element_set_state(pipeline, GST_STATE_PLAYING) to start             */
/* gst_element_set_state(pipeline, GST_STATE_NULL) to stop                 */

/* inih replacements for INI file APIs */
/* ini_parse("~/.config/lego-loco/loco.ini", handler, user_data)           */
/* Custom ini_get_int / ini_get_string / ini_write accessors                */

/* POSIX replacements for user name lookup */
#include <pwd.h>
#include <unistd.h>
/* struct passwd *pw = getpwuid(getuid()); pw->pw_name                     */

#endif /* LOCO_LINUX */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NETWORK_H */
