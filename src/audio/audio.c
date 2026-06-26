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

/*
 * loco_audio.c -- Audio / DirectSound subsystem for LEGO Loco (1998)
 *
 * Covers: INI config wrappers, user-profile save/load, MCI video player
 * for intro videos and background music, PlaySoundA background music,
 * DirectSound (DSOUND.DLL Ordinal_1/2) sound-effect system, and the
 * tile-grid sound-attachment / pick-up-sound pipeline.
 *
 * Global audio manager object: DAT_004aa4a8 (allocated 0x124 = 292 bytes)
 * Global INI object:           DAT_004a9eec
 * Global sound table:          DAT_004855e8
 * Background music path:       "%svideo\music.wav"
 * Intro video paths:           "%svideo\IgSpin.avi", "%svideo\legoSpin.avi"
 *
 * Windows subsystems used:
 *   DSOUND.DLL  - Ordinal_1 (DirectSoundCreate), Ordinal_2 (DirectSoundEnumerate)
 *   WINMM.DLL   - PlaySoundA, mciSendCommandA
 *   MSVFW32.DLL - MCIWndRegisterClass
 *   USER32.DLL  - SendMessageA, CreateWindowExA, PostMessageA, SetTimer ...
 *   KERNEL32    - CreateFileA, ReadFile, WriteFile, GetUserNameA, LoadStringA ...
 *
 * Linux port strategy: SDL2_mixer (music), OpenAL (sfx), GStreamer (video)
 * See full replacement notes in linux_replacement field.
 */

#include <windows.h>
#include <mmsystem.h>   /* PlaySoundA, mciSendCommandA */
#include <vfw.h>        /* MCIWndRegisterClass, MCIWnd messages */
#include <dsound.h>     /* DirectSoundCreate, IDirectSoundBuffer */

/* =========================================================================
 * Forward declarations of internal helpers
 * ========================================================================= */
static void  FUN_00465cd0(void *p);               /* operator delete           */
static void *FUN_00465ce0(size_t n);              /* operator new              */
static void  FUN_00467ea0(uint v, char *buf, int base); /* custom itoa          */

/* =========================================================================
 * Section 1: INI-File wrapper (CIniFile)
 *
 * The game stores all persistent settings (audio on/off, username, slot IDs)
 * in a Windows .INI file.  DAT_004a9eec is the singleton CIniFile object.
 * The INI path is stored at this+4.
 *
 * Key sections used by the audio subsystem:
 *   [MOUSE]       -- username stored here (Setting1/2/3 per task context)
 *   [CLIENT]      -- NextId = next available user-slot number (1..999)
 *   [ScreenSaver] -- Sound = 0 means music plays during screensaver
 *   [Sound]       -- reserved for future audio settings
 * ========================================================================= */

/*
 * CIniFile_Construct (FUN_00452d50)
 *
 * Sets the vtable pointer of a freshly-allocated CIniFile object.
 * No Windows APIs called.
 */
void __fastcall CIniFile_Construct(void **pObj)
{
    /* WIN32: none */
    /* LINUX: none */
    *pObj = &vtable_CIniFile;   /* PTR_LAB_004784bc */
}

/*
 * CIniFile_ReadInt (FUN_00452d60)
 *
 * Reads an integer value from [section] key in the INI file.
 * Returns defaultVal if the key is absent.
 *
 * WIN32: GetPrivateProfileIntA
 * LINUX: inih ini_get_int(cfg, section, key, defaultVal)
 */
int __thiscall CIniFile_ReadInt(void *this,
                                LPCSTR section,
                                LPCSTR key,
                                INT    defaultVal)
{
    /* this+4 = path to INI file */
    return GetPrivateProfileIntA(section, key, defaultVal,
                                 (LPCSTR)((char *)this + 4));
}

/*
 * CIniFile_ReadString (FUN_00452d80)
 *
 * Reads a string value from [section] key in the INI file.
 * Used to retrieve the stored player username from the [MOUSE] section.
 *
 * WIN32: GetPrivateProfileStringA
 * LINUX: inih ini_get_string(cfg, section, key, defaultVal, outBuf, bufSize)
 */
void __thiscall CIniFile_ReadString(void   *this,
                                    LPCSTR  section,
                                    LPCSTR  key,
                                    LPCSTR  defaultVal,
                                    LPSTR   outBuf,
                                    DWORD   bufSize)
{
    GetPrivateProfileStringA(section, key, defaultVal, outBuf, bufSize,
                             (LPCSTR)((char *)this + 4));
}

/*
 * CIniFile_WriteInt (FUN_00452db0)
 *
 * Converts 'value' to a decimal string then writes it to [section] key.
 * Used to persist the incremented CLIENT/NextId counter.
 *
 * WIN32: WritePrivateProfileStringA
 * LINUX: custom ini_set_int() + fflush
 */
void __thiscall CIniFile_WriteInt(void   *this,
                                  LPCSTR  section,
                                  LPCSTR  key,
                                  uint    value)
{
    char buf[100];
    FUN_00467ea0(value, buf, 10);   /* custom itoa, base 10 */
    WritePrivateProfileStringA(section, key, buf,
                               (LPCSTR)((char *)this + 4));
}

/*
 * CIniFile_WriteString (FUN_00452df0)
 *
 * Writes a raw string to [section] key in the INI file.
 *
 * WIN32: WritePrivateProfileStringA
 * LINUX: custom ini_set_string() + fflush
 */
void __thiscall CIniFile_WriteString(void   *this,
                                     LPCSTR  section,
                                     LPCSTR  key,
                                     LPCSTR  value)
{
    WritePrivateProfileStringA(section, key, value,
                               (LPCSTR)((char *)this + 4));
}

/* =========================================================================
 * Section 2: User Profile save/load (CUserProfile)
 *
 * Each player has a 0x120-byte binary .usr file under the game save
 * directory (path format: "%s%s.usr" using DAT_004a99c8 as base dir
 * and the username as filename stem).
 *
 * Layout of the 0x120 save block (read/written by CreateFileA+Read/WriteFile):
 *   +0x00  uint16 magic        = 0x0066 (file format version)
 *   +0x02  char[10] username
 *   +0x14  uint32 reserved
 *   +0x18  uint32 clientId     (1..999)
 *   +0x1c  uint32 saveCounter  (0..9999, wraps)
 *   +0x20  char[] lastShotName (formatted as "%03d_%04d")
 * ========================================================================= */

/*
 * CUserProfile_LoadFromFile (FUN_004530c0)
 *
 * Builds the path "%s%s.usr" and opens it read-only.
 * Reads 0x120 bytes into the profile buffer starting at param_1+4.
 * Validates magic == 0x66; if wrong, assigns a fresh clientId from
 * CLIENT/NextId and rewrites the file.
 *
 * WIN32: wsprintfA, CreateFileA, ReadFile, WriteFile, CloseHandle
 * LINUX: snprintf + open/read/write/close (POSIX unistd.h)
 */
int __fastcall CUserProfile_LoadFromFile(int pProfile)
{
    char path[1284];
    HANDLE hFile;
    DWORD  bytesRW;
    short *pMagic  = (short *)(pProfile + 4);   /* file format version  */
    char  *pName   = (char *) (pProfile + 6);   /* username field       */

    /* Build path: gameSaveDir + username + ".usr" */
    /* WIN32: */ wsprintfA(path, "%s%s.usr", &DAT_004a99c8, pName);

    /* WIN32: */ hFile = CreateFileA(path,
                     GENERIC_READ,           /* 0x80000000 */
                     FILE_SHARE_READ,        /* 0x01       */
                     NULL,
                     OPEN_EXISTING,          /* 0x03       */
                     FILE_FLAG_NO_BUFFERING, /* 0x08000000 */
                     NULL);
    /* LINUX:    fd = open(path, O_RDONLY);  */

    if (hFile == INVALID_HANDLE_VALUE)
        return 0;   /* file not found -> caller treats as new user */

    /* WIN32: */ ReadFile(hFile, pMagic, 0x120, &bytesRW, NULL);
    /* LINUX:    read(fd, pMagic, 0x120);   */

    /* WIN32: */ CloseHandle(hFile);

    if (*pMagic != 0x66) {
        /* Corrupt or wrong-version file: reassign a fresh slot */
        *pMagic = 0x66;
        *pName  = '\0';
        *(uint *)(pProfile + 0x14) = 0;
        *(uint *)(pProfile + 0x18) = 0;
        *(uint *)(pProfile + 0x1c) = 0;

        int nextId = CIniFile_ReadInt(DAT_004a9eec, "CLIENT", "NextId", 0);
        if (nextId > 999) nextId = 1;
        CIniFile_WriteInt(DAT_004a9eec, "CLIENT", "NextId", nextId + 1);
        *(int *)(pProfile + 0x18) = nextId;

        /* Rebuild path with now-empty username and rewrite */
        /* WIN32: */ wsprintfA(path, "%s%s.usr", &DAT_004a99c8, pName);
        hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                            CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, pMagic, 0x120, &bytesRW, NULL);
            CloseHandle(hFile);
        }
    }
    return 1;
}

/*
 * CUserProfile_SaveToFile (FUN_004532a0)
 *
 * Writes the 0x120-byte profile block to disk.
 *
 * WIN32: wsprintfA, CreateFileA, WriteFile, CloseHandle
 * LINUX: snprintf + open(O_WRONLY|O_CREAT|O_TRUNC,0644) + write + close
 */
void __fastcall CUserProfile_SaveToFile(int pProfile)
{
    char   path[1284];
    HANDLE hFile;
    DWORD  written;

    /* WIN32: */ wsprintfA(path, "%s%s.usr", &DAT_004a99c8,
                           (char *)(pProfile + 6));
    hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, (void *)(pProfile + 4), 0x120, &written, NULL);
        CloseHandle(hFile);
    }
}

/*
 * CUserProfile_MakeScreenshotName (FUN_00453320)
 *
 * Formats the next screenshot filename as "%03d_%04d" (clientId, saveCounter)
 * into the buffer at pProfile+0x20, increments saveCounter (wraps at 9999),
 * and flushes the profile to disk.
 *
 * Returns: pointer to the name buffer (pProfile+0x20).
 *
 * WIN32: wsprintfA, CreateFileA, WriteFile, CloseHandle
 * LINUX: snprintf + POSIX file I/O
 */
char *__fastcall CUserProfile_MakeScreenshotName(int pProfile)
{
    char   path[1284];
    HANDLE hFile;
    DWORD  written;
    int    nextCounter;

    /* Format the screenshot name into the profile buffer */
    /* WIN32: */ wsprintfA((char *)(pProfile + 0x20), "%03d_%04d",
                           *(uint *)(pProfile + 0x18),  /* clientId    */
                           *(uint *)(pProfile + 0x1c)); /* saveCounter */

    /* Increment and wrap the counter */
    nextCounter = *(int *)(pProfile + 0x1c) + 1;
    if (nextCounter > 9999) nextCounter = 0;
    *(int *)(pProfile + 0x1c) = nextCounter;

    /* Flush to disk */
    /* WIN32: */ wsprintfA(path, "%s%s.usr", &DAT_004a99c8,
                           (char *)(pProfile + 6));
    hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, (void *)(pProfile + 4), 0x120, &written, NULL);
        CloseHandle(hFile);
    }
    return (char *)(pProfile + 0x20);
}

/*
 * CUserProfile_Construct (FUN_00452e10)
 *
 * Constructor for the user-profile object stored at DAT_004aa4a8 (and
 * similar profile objects for each player slot).
 *
 * Algorithm:
 *   1. Set vtable, magic=0x66, zero clientId / saveCounter.
 *   2. Read stored username from INI [MOUSE] section via CIniFile_ReadString.
 *   3. If INI returns empty string, fall back to GetUserNameA (Windows login).
 *   4. If still empty, default to "LEGO LOCO".
 *   5. Compare retrieved name with previously stored name at this+6.
 *   6. If different: reload .usr file, assign CLIENT/NextId, set isNewUser.
 *   7. If DAT_004aa4a8 (global audio manager) is live, trigger audio/UI refresh.
 *
 * WIN32: GetUserNameA (advapi32), GetPrivateProfileStringA (via INI helper)
 * LINUX: getpwuid(getuid())->pw_name  for step 3
 */
void *__fastcall CUserProfile_Construct(uint32_t *pProfile)
{
    char    nameBuffer[16];
    DWORD   nameLen = 0x0d;

    /* Set vtable pointer */
    *pProfile = (uint32_t)&vtable_CUserProfile;   /* PTR_LAB_004784c0 */

    /* Initialise magic word and wipe IDs */
    *(uint16_t *)(pProfile + 1) = 0x66;
    *(char  *)((char *)pProfile + 6) = '\0';
    pProfile[5] = 0; pProfile[6] = 0; pProfile[7] = 0;
    *(char *)((char *)pProfile + 0x120) = 0;   /* isNewUser = false */

    /* Step 2: read username from INI [MOUSE] section */
    CIniFile_ReadString(DAT_004a9eec,
                        &DAT_0047e734,   /* section = "MOUSE"    */
                        &DAT_0047e73c,   /* key     = "Setting1" */
                        &DAT_004851d0,   /* default = ""         */
                        nameBuffer, 0x0d);

    if (nameBuffer[0] == '\0') {
        /* Step 3: fall back to Windows login name */
        /* WIN32: */ GetUserNameA(nameBuffer, &nameLen);
        /* LINUX:
           struct passwd *pw = getpwuid(getuid());
           if (pw && pw->pw_name) strncpy(nameBuffer, pw->pw_name, 12);
        */

        if (nameBuffer[0] == '\0') {
            /* Step 4: hard-coded default */
            memcpy(nameBuffer, "LEGO LOCO", sizeof("LEGO LOCO"));
        }
    }

    /* Step 5: compare with already-stored name at this+6 */
    if (strcmp((char *)pProfile + 6, nameBuffer) != 0) {
        /* Copy new username into the profile */
        memcpy((char *)pProfile + 6, nameBuffer, strlen(nameBuffer) + 1);

        /* Invalidate any active audio timer */
        if (DAT_004fd3b4 != 0)
            thunk_FUN_00401c90(DAT_004fd3b4);

        /* Step 6: try to load the .usr save file */
        if (!CUserProfile_LoadFromFile((int)pProfile)) {
            /* New user: assign slot */
            int nextId = CIniFile_ReadInt(DAT_004a9eec, "CLIENT", "NextId", 0);
            if (nextId > 999) nextId = 1;
            CIniFile_WriteInt(DAT_004a9eec, "CLIENT", "NextId", nextId + 1);
            pProfile[6] = nextId;
            *(char *)((char *)pProfile + 0x120) = 1;   /* isNewUser = true */
        } else {
            *(char *)((char *)pProfile + 0x120) = 0;
        }

        /* Step 7: audio/UI refresh if global manager is live */
        if (DAT_004aa4a8 != 0) {
            FUN_0041a0e0(DAT_004fd380);
            FUN_00445170();
        }
    }
    return pProfile;
}

/* =========================================================================
 * Section 3: MCI Video Player (CMciVideoPlayer)
 *
 * Used to play intro splash videos (IgSpin.avi, legoSpin.avi) and the
 * background music WAV file (music.wav) through the MCIWnd window class.
 *
 * MCIWnd messages used:
 *   0x499 = MCIWNDM_OPEN     -- open a media file
 *   0x48F = MCIWNDM_PUT_DEST -- set destination (display) rect
 *   0x464 = MCIWNDM_GETDEVICEID -- get MCI device ID for mciSendCommandA
 *   0x804 = MCIWNDM_STOP     -- stop playback
 *   0x10  = WM_CLOSE         -- close/destroy the MCIWnd window
 * ========================================================================= */

/*
 * CMciVideoPlayer_PlayInWindow (FUN_00454380)
 *
 * Core video/audio playback function.
 *
 * 1. Stops any currently playing MCIWnd (MCIWNDM_STOP + WM_CLOSE).
 * 2. Registers the MCIWnd window class.
 * 3. Creates a child window of style WS_CHILD|WS_VISIBLE|MCIWNDF_NOPLAYBAR.
 * 4. Opens the media file (MCIWNDM_OPEN).
 * 5. Sizes the window (MCIWNDM_PUT_DEST).
 * 6. Retrieves the MCI device ID (MCIWNDM_GETDEVICEID).
 * 7. Calls mciSendCommandA(MCI_SETAUDIO, 0x1000001) to set audio volume.
 *
 * Parameters:
 *   this      -- CMciVideoPlayer object
 *   pFilePath -- LPARAM pointing to null-terminated media file path
 *   pRect     -- int[4] = { left, top, right, bottom } of desired display area
 *
 * WIN32: SendMessageA, MCIWndRegisterClass, CreateWindowExA, mciSendCommandA
 * LINUX: GStreamer gst_parse_launch("playbin uri=...") embedded in SDL window;
 *        or libVLC libvlc_media_player_set_xwindow + libvlc_media_player_play.
 */
void __thiscall CMciVideoPlayer_PlayInWindow(void   *this,
                                             LPARAM  pFilePath,
                                             int    *pRect)
{
    HWND hWnd;
    MCIDEVICEID mciId;
    /* MCI_SETAUDIO_PARMS: dwCallback, dwVolume, dwSpeed, dwOver, dwTo, dwItem */
    struct { DWORD v[6]; } mciAudio;
    int destRect[2];   /* width, height for MCIWNDM_PUT_DEST */

    uint audioVolume = *(uint *)((char *)this + 0x18);

    /* Zero the audio params */
    mciAudio.v[0] = mciAudio.v[1] = mciAudio.v[2] = 0;

    /* 1. Stop any existing MCIWnd */
    if (*(HWND *)((char *)this + 4) != NULL) {
        /* WIN32: */
        SendMessageA(*(HWND *)((char *)this + 4), 0x804, 0, 0); /* MCIWNDM_STOP  */
        SendMessageA(*(HWND *)((char *)this + 4), 0x10,  0, 0); /* WM_CLOSE      */
        *(HWND *)((char *)this + 4) = NULL;
    }

    /* 2. Register MCIWnd window class */
    /* WIN32: */
    MCIWndRegisterClass();
    /* LINUX:  gst_init(NULL, NULL); or libvlc_new(0, NULL); */

    /* 3. Create MCIWnd child window
     *    style 0x4000400B = WS_CHILD|WS_VISIBLE|MCIWNDF_NOPLAYBAR|MCIWNDF_NOTIFYMODE
     */
    /* WIN32: */
    hWnd = CreateWindowExA(0,
               "MCIWndClass",
               NULL,               /* window title (none) */
               0x4000400B,         /* child + visible + no play bar  */
               pRect[0],           /* left   */
               pRect[1],           /* top    */
               pRect[2] - pRect[0],/* width  */
               pRect[3] - pRect[1],/* height */
               *(HWND *)((char *)this + 0x18), /* parent window */
               NULL,               /* no menu       */
               *(HINSTANCE *)((char *)this + 0x1c),
               NULL);
    /* LINUX:  create GStreamer bus + video-overlay on SDL window */

    *(HWND *)((char *)this + 4) = hWnd;

    if (hWnd != NULL) {
        /* 4. Open the media file */
        /* WIN32: */ SendMessageA(hWnd, 0x499, 0, pFilePath); /* MCIWNDM_OPEN */

        /* 5. Set destination rectangle */
        destRect[0] = 0;
        destRect[1] = pRect[2] - pRect[0];   /* width  */
        /* iStack_4 (height) placed right after destRect[1] on the stack */
        int height = pRect[3] - pRect[1];
        /* The original passes a pointer to a 6-int struct on stack:
           [0]=0, [1]=0, [2]=0, [3]=0, [4]=0(top), [5]=width, iStack=height */
        SendMessageA(hWnd, 0x48F, 0, (LPARAM)&destRect); /* MCIWNDM_PUT_DEST */

        /* 6. Get the MCI device ID */
        mciId = (MCIDEVICEID)SendMessageA(hWnd, 0x464, 0, 0); /* MCIWNDM_GETDEVICEID */

        /* 7. Set audio volume via mciSendCommandA MCI_SETAUDIO (0x806)
         *    Flags 0x1000001 = MCI_NOTIFY | MCI_SETAUDIO_VOLUME
         *    dwItem = audioVolume (lower 16 bits)
         */
        mciAudio.v[0] = audioVolume & 0xFFFF;
        /* WIN32: */
        mciSendCommandA(mciId, 0x806, 0x1000001, (DWORD_PTR)&mciAudio);
        /* LINUX:  g_object_set(playbin, "volume", (double)audioVolume/100.0, NULL);
         *         or libvlc_audio_set_volume(player, audioVolume); */
    }
}

/*
 * CMciVideoPlayer_Construct (FUN_00454250)
 *
 * Creates a CMciVideoPlayer, verifies the media file exists (CreateFileA),
 * gets the parent window rect (GetClientRect), and calls
 * CMciVideoPlayer_PlayInWindow to start playback.
 * Posts WM_USER+0x3b9 to the parent if the file is not found.
 *
 * WIN32: CreateFileA, CloseHandle, GetClientRect, PostMessageA
 * LINUX: access(path, F_OK) for existence check; SDL_GetWindowSize for rect
 */
void *__thiscall CMciVideoPlayer_Construct(void  *this,
                                           HWND   hParent,
                                           HINSTANCE hInstance,
                                           char  *pFilePath)
{
    HANDLE   hCheck;
    RECT     clientRect;
    int      rect[4];

    /* Set vtable (PTR_FUN_004784c4) */
    *(void **)this = &vtable_CMciVideoPlayer;
    *(HWND  *)((char *)this + 4)    = NULL;
    *(HWND  *)((char *)this + 0x18) = hParent;
    *(HINSTANCE *)((char *)this + 0x1c) = hInstance;

    /* Copy file path into this+0x20 */
    strcpy((char *)this + 0x20, pFilePath);

    /* Verify the file exists */
    /* WIN32: */
    hCheck = CreateFileA(pFilePath,
                         GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    /* LINUX:  if (access(pFilePath, F_OK) != 0) ... */
    if (hCheck == INVALID_HANDLE_VALUE) {
        PostMessageA(hParent, WM_USER + 0x3b9, 0, 0);
        return this;
    }
    CloseHandle(hCheck);

    /* Get parent client area for positioning */
    /* WIN32: */ GetClientRect(hParent, &clientRect);
    /* LINUX:  SDL_GetWindowSize(sdlWindow, &w, &h); */

    /* Default video rect: 0,0 -> 0x280 x 0x1e0 (640x480) */
    rect[0] = 0;
    rect[1] = 0x280;  /* right  */
    rect[2] = 0;
    rect[3] = 0x1e0;  /* bottom */
    /* FUN_00425a50 scales rect to fit clientRect -- not shown here */

    CMciVideoPlayer_PlayInWindow(this, (LPARAM)pFilePath, rect);
    return this;
}

/*
 * CMciVideoPlayer_Destruct (FUN_00454330)
 *
 * Stops and closes the MCIWnd, optionally frees memory.
 *
 * WIN32: SendMessageA (MCIWNDM_STOP=0x804, WM_CLOSE=0x10)
 * LINUX: gst_element_set_state(pipeline, GST_STATE_NULL); gst_object_unref(pipeline);
 */
void *__thiscall CMciVideoPlayer_Destruct(void *this, BYTE freeMemory)
{
    *(void **)this = &vtable_CMciVideoPlayer;
    if (*(HWND *)((char *)this + 4) != NULL) {
        SendMessageA(*(HWND *)((char *)this + 4), 0x804, 0, 0); /* MCIWNDM_STOP */
        SendMessageA(*(HWND *)((char *)this + 4), 0x10,  0, 0); /* WM_CLOSE     */
        *(HWND *)((char *)this + 4) = NULL;
    }
    if (freeMemory & 1)
        FUN_00465cd0(this);   /* operator delete */
    return this;
}

/*
 * CMciVideoPlayer_Stop (FUN_004544a0)
 *
 * Non-destructive stop: sends MCIWNDM_STOP + WM_CLOSE and nulls the handle.
 *
 * WIN32: SendMessageA
 * LINUX: gst_element_set_state(pipeline, GST_STATE_NULL)
 */
void __fastcall CMciVideoPlayer_Stop(int pPlayer)
{
    if (*(HWND *)(pPlayer + 4) != NULL) {
        SendMessageA(*(HWND *)(pPlayer + 4), 0x804, 0, 0); /* MCIWNDM_STOP */
        SendMessageA(*(HWND *)(pPlayer + 4), 0x10,  0, 0); /* WM_CLOSE     */
        *(HWND *)(pPlayer + 4) = NULL;
    }
}

/* =========================================================================
 * Section 4: Audio State Machine (CAudioStateMachine)
 *
 * FUN_004208f0 drives the high-level game audio state.  The state variable
 * lives at this+0xe8.  States:
 *   0 = initial / idle
 *   1 = music stopped (transitioning to menu)
 *   2 = pause / hide overlay
 *   3 = video playing (MusicController -> Play)
 *   4 = video fullscreen (no overlay)
 *   5 = video with audio track
 *   6 = return to main menu
 *   7 = in-game: play music.wav via PlaySoundA, stop video player
 * ========================================================================= */

/*
 * CAudioStateMachine_SetState (FUN_004208f0)
 *
 * Changes the audio/game-mode state and reacts accordingly.
 * Key audio transitions:
 *   -> state 1: stop all music (PlaySoundA(NULL, 0, SND_PURGE=0))
 *   -> state 7: play "%svideo\music.wav" (PlaySoundA, SND_FILENAME|SND_ASYNC=9)
 *   -> state 3: call vtable[1] on MusicController (starts video audio)
 *   -> state 6: stop both audio controllers, switch to menu
 *
 * WIN32: ShowWindow, SetWindowLongA, PlaySoundA, wsprintfA
 * LINUX:
 *   stop:  Mix_HaltMusic() or Mix_HaltChannel(-1)
 *   play:  Mix_LoadMUS(path); Mix_PlayMusic(music, 1);
 *   video: gst_element_set_state(pipeline, GST_STATE_PLAYING)
 */
void __thiscall CAudioStateMachine_SetState(void *this, int newState)
{
    int  prevState = *(int *)((char *)this + 0xe8);
    char musicPath[1284];

    *(int *)((char *)this + 0xe8) = newState;

    switch (newState) {
    case 1:
        /* Stop music and hide video window */
        /* WIN32: */ PlaySoundA(NULL, NULL, 0);
        /* LINUX:    Mix_HaltMusic(); */
        ShowWindow(*(HWND *)((char *)this + 0x20c), SW_HIDE);
        return;

    case 2:
        /* Pause: hide window and stop sound controller */
        ShowWindow(*(HWND *)((char *)this + 0x20c), SW_HIDE);
        /* call vtable[2] (Stop) on MusicController */
        (*(void(**)(void))(*(int *)((char *)this + 0x21c))[2])();
        if (prevState == 4 || prevState == 5)
            (*(void(**)(void))(*(int *)((char *)this + 0x220))[1])();
        return;

    case 3:
        /* Start video audio track */
        /* call vtable[1] (Play) on MusicController */
        (*(void(**)(void))(*(int *)((char *)this + 0x21c))[1])();
        if (*(char *)(DAT_004fd3a8 + 0x18) == '\0') {
            *(int *)((char *)this + 0xe8) = 4;
            (*(void(**)(void))(*(int *)((char *)this + 0x220))[2])();
        } else {
            *(int *)((char *)this + 0xe8) = 5;
            (*(void(**)(void))(*(int *)((char *)this + 0x220))[2])();
        }
        return;

    case 4:
    case 5:
        ShowWindow(*(HWND *)((char *)this + 0x20c), SW_HIDE);
        (*(void(**)(void))(*(int *)((char *)this + 0x220))[2])();
        return;

    case 6:
        /* Return to menu: stop SFX + music controllers, optionally train audio */
        (*(void(**)(void))(*(int *)((char *)this + 0x220))[1])();
        (*(void(**)(void))(*(int *)((char *)this + 0x21c))[1])();
        if (*(char *)(DAT_004fd3a8 + 7) != '\0')
            FUN_0043d2b0(DAT_004fd3ac, 1);   /* stop train audio */
        FUN_004616c0(DAT_004fd398, 1);
        FUN_00408130((void *)0x1);            /* switch to main menu state */
        return;

    case 7:
        /* In-game: stop video player, start background music */
        if (*(int *)((char *)this + 0x210) != 0) {
            /* Restore saved window-long (WS_EX_LAYERED etc.) */
            /* WIN32: */
            SetWindowLongA(*(HWND *)(*(int *)((char *)this + 0x210) + 4),
                           GWL_WNDPROC,
                           *(LONG *)((char *)this + 0x218));
            CMciVideoPlayer_Stop(*(int *)((char *)this + 0x210));
            /* Free the video player object via vtable[0] (destructor) */
            if (*(void **)((char *)this + 0x210) != NULL)
                (*(void(**)(int,int))**(void***)((char *)this + 0x210))(1, 0);
            *(int *)((char *)this + 0x210) = 0;
            *(int *)((char *)this + 0xe8)  = 99;   /* sentinel: was showing video */
        }
        if (prevState == 0) FUN_0045b7e0();   /* first-boot hook */
        if (prevState == 1 || prevState == 0) {
            /* Play background music WAV asynchronously */
            /* WIN32: */
            wsprintfA(musicPath, "%svideo\\music.wav", &DAT_004a99c8);
            PlaySoundA(musicPath, NULL, SND_FILENAME | SND_ASYNC);
            /* LINUX:
               snprintf(musicPath, sizeof(musicPath), "%svideo/music.wav", saveDir);
               Mix_Music *m = Mix_LoadMUS(musicPath);
               Mix_PlayMusic(m, 1);
            */
        }
        (*(void(**)(void))(*(int *)((char *)this + 0x220))[1])();
        (*(void(**)(void))(*(int *)((char *)this + 0x21c))[1])();
        return;
    }
}

/* =========================================================================
 * Section 5: Screensaver Sound (CScreenSaver_PlayMusic)
 * ========================================================================= */

/*
 * CScreenSaver_PlayMusic (FUN_004480c0)
 *
 * Called when the screensaver countdown (DAT_004a9918) reaches 1.
 * Reads the INI key [ScreenSaver] Sound; if 0, plays music.wav.
 *
 * WIN32: GetPrivateProfileIntA (via CIniFile_ReadInt), wsprintfA, PlaySoundA
 * LINUX: custom INI read; Mix_LoadMUS + Mix_PlayMusic
 */
int __fastcall CScreenSaver_PlayMusic(int param_1)
{
    char musicPath[1284];
    int  soundDisabled;
    int  result;

    if (DAT_004a9918 - 1 != 0)
        return 1;   /* countdown not yet at 1 */

    FUN_004487f0(param_1);   /* screensaver init hook */

    /* Read [ScreenSaver] Sound  (0 = play music, 1 = silent) */
    /* WIN32: */ soundDisabled = CIniFile_ReadInt(DAT_004a9eec,
                                                  "ScreenSaver", "Sound", 0);
    /* LINUX:    soundDisabled = ini_get_int(cfg, "ScreenSaver", "Sound", 0); */

    if (soundDisabled == 0) {
        /* WIN32: */ wsprintfA(musicPath, "%svideo\\music.wav", &DAT_004a99c8);
        /* WIN32: */ result = PlaySoundA(musicPath, NULL, SND_FILENAME | SND_ASYNC);
        /* LINUX:
           snprintf(musicPath, sizeof(musicPath), "%svideo/music.wav", saveDir);
           Mix_Music *m = Mix_LoadMUS(musicPath);
           result = (Mix_PlayMusic(m, 1) == 0) ? 1 : 0;
        */
        return result;
    }
    return 1;
}

/*
 * CGame_StopAllSounds (FUN_0045e090)
 *
 * Stops all PlaySoundA music (PlaySoundA(NULL,0,0)), clears the render
 * target to black (GetStockObject + FillRect), then creates the title dialog.
 *
 * WIN32: PlaySoundA, GetStockObject, FillRect, GetSystemMetrics, VirtualAlloc
 * LINUX: Mix_HaltMusic(); SDL_SetRenderDrawColor(0,0,0,255); SDL_RenderClear();
 */
void CGame_StopAllSounds(void)
{
    /* Stop any currently playing WAV/music */
    /* WIN32: */ PlaySoundA(NULL, NULL, 0);
    /* LINUX:    Mix_HaltMusic(); Mix_HaltChannel(-1); */

    /* Clear screen to black */
    /* WIN32: */
    HBRUSH blackBrush = GetStockObject(BLACK_BRUSH);
    FillRect(/* HDC from caller's ESI register */0, (RECT *)&DAT_00485220, blackBrush);
    /* LINUX:
       SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
       SDL_RenderClear(renderer);
    */

    /* ... create title dialog and center it using GetSystemMetrics ... */
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    /* LINUX: SDL_GetDisplayMode(0, &mode); mode.w / mode.h; */
}

/* =========================================================================
 * Section 6: DirectSound Sound-Effect System
 *
 * DSOUND.DLL Ordinal_1 = DirectSoundCreate is called deep in FUN_0040d500,
 * which is invoked from CSoundEffect_Construct (FUN_0044be50).
 * DSOUND.DLL Ordinal_2 = DirectSoundEnumerate used during audio init.
 *
 * Sound resource names are stored as Windows string-table entries.
 * LoadStringA retrieves the name; the name is then used to locate the WAV
 * data in the game's resource archive.
 * ========================================================================= */

/*
 * CSoundTable_LoadSoundById (FUN_00446ea0)
 *
 * Looks up soundId in the global sound resource table (DAT_004855e8).
 * If the slot is unloaded (value 0), calls GetModuleHandleA + LoadStringA
 * to retrieve the resource name, optionally mapping to a language variant
 * (game modes 1-9 add fixed offsets to soundId before loading).
 * Calls FUN_00446840 to parse the name and initialise the slot.
 *
 * WIN32: GetModuleHandleA, LoadStringA
 * LINUX: custom resource bundle lookup (data/sounds.txt or binary archive)
 */
int __thiscall CSoundTable_LoadSoundById(void *this, int soundId)
{
    char   nameBuf[264];
    int    nameLen;
    HMODULE hMod;
    uint   lookupId;

    if (soundId < 0 || soundId > 0x3FFF)
        return 0;

    int *pSlot = *(int **)((char *)this + soundId * 4 + 0x2c);
    if (pSlot == NULL) return 0;
    if (*pSlot != 0) return (*pSlot == -1) ? 0 : *pSlot;

    /* Slot not yet loaded -- use LoadStringA to get the resource name */
    /* Language variant: game modes 1-9 add fixed offsets */
    lookupId = soundId;   /* (simplified; actual code has switch for offsets) */

    /* WIN32: */
    hMod = GetModuleHandleA(NULL);
    nameLen = LoadStringA(hMod, lookupId, nameBuf, 0x104);
    /* LINUX:
       look up soundId in sounds_table.txt loaded at startup
    */

    if (nameLen != 0)
        FUN_00446840(this, soundId, (int)nameBuf);
    else
        *pSlot = -1;   /* mark as unavailable */

    pSlot = *(int **)((char *)this + soundId * 4 + 0x2c);
    return (*pSlot && *pSlot != -1) ? *pSlot : 0;
}

/*
 * CSoundEffect_Construct (FUN_0044be50)
 *
 * Initialises a CSoundEffect object wrapping one DirectSound buffer.
 * Calls FUN_0040d500 to create and fill the IDirectSoundBuffer (which
 * in turn calls DSOUND.DLL Ordinal_1 = DirectSoundCreate if not yet
 * initialised).  Configures pan, volume, 3D position, and registers
 * with the global mixer table.
 *
 * WIN32 (indirect): DirectSoundCreate, IDirectSoundBuffer::SetCurrentPosition,
 *                   IDirectSoundBuffer::Play
 * LINUX: alGenBuffers + alBufferData + alGenSources + alSourcePlay (OpenAL)
 *     OR Mix_LoadWAV + Mix_PlayChannel (SDL2_mixer)
 */
void *__thiscall CSoundEffect_Construct(void      *this,
                                        int        soundId,
                                        uint32_t   flags,
                                        char       isLooping,
                                        uint8_t    layerIndex)
{
    void *pPosData;
    void *pBufWrapper;

    /* Initialise replay-timer slots to 0xFFFF (no delay) */
    *(uint16_t *)((char *)this + 0x2e) = 0xFFFF;
    *(uint16_t *)((char *)this + 0x30) = 0xFFFF;
    *(uint16_t *)((char *)this + 0x32) = 0xFFFF;
    *(uint16_t *)((char *)this + 0x34) = 0xFFFF;

    *(uint8_t  *)((char *)this + 0x88) = layerIndex;
    *(uint32_t *)((char *)this + 4)    = flags;
    *(void    **)this = &vtable_CSoundEffect;   /* PTR_FUN_0047836c */

    /* Zero position/state fields */
    *(uint8_t  *)((char *)this + 0x5a) = 0;
    *(uint8_t  *)((char *)this + 0x90) = 0;
    *(uint8_t  *)((char *)this + 0x2c) = 0;
    *(uint32_t *)((char *)this + 0x8c) = 0;
    *(uint32_t *)((char *)this + 0x68) = 0;
    *(uint32_t *)((char *)this + 0x70) = 0;
    memset((char *)this + 0x38, 0, 8 * 4);

    /* Allocate 3D position data block */
    pPosData = FUN_00465ce0(0x20);
    if (pPosData)
        *(uint32_t *)((char *)this + 0x20) = (uint32_t)FUN_0040b500(pPosData, isLooping);
    else
        *(uint32_t *)((char *)this + 0x20) = 0;

    /* Allocate and load DirectSound buffer wrapper (0x450 bytes) */
    /* This eventually calls DSOUND.DLL Ordinal_1 (DirectSoundCreate) */
    pBufWrapper = FUN_00465ce0(0x450);
    if (pBufWrapper)
        *(int *)((char *)this + 0x10) = (int)FUN_0040d500(pBufWrapper, soundId, 2, isLooping);
    /* LINUX:
       ALuint alBuf, alSrc;
       alGenBuffers(1, &alBuf);
       alBufferData(alBuf, format, pcmData, pcmSize, sampleRate);
       alGenSources(1, &alSrc);
       alSourcei(alSrc, AL_BUFFER, alBuf);
       if (isLooping) alSourcei(alSrc, AL_LOOPING, AL_TRUE);
    */

    /* Configure pan, volume, mixer registration */
    /* ... (FUN_0044d740 for 3D pos, FUN_00440610 for mixer) ... */

    if (!isLooping) {
        *(uint32_t *)((char *)this + 100) = 2;   /* playState = playing */
        FUN_0044d500(this, '\0');                  /* start one-shot      */
    } else {
        *(uint32_t *)((char *)this + 0x60) = 2;
        FUN_0044d500(this, '\x01');                /* start looping       */
    }
    return this;
}

/*
 * CGameObject_HasActiveSound (FUN_0044bd10)
 *
 * Returns 1 if the game object at pObj is currently emitting a sound
 * (state byte at pObj+0x63a is 1=queued, 2=playing, 3=looping, 4=finishing).
 *
 * LINUX: check alGetSourcei(alSrc, AL_SOURCE_STATE) == AL_PLAYING
 */
int __fastcall CGameObject_HasActiveSound(int pObj)
{
    char state = *(char *)(pObj + 0x63a);
    return (state == 1 || state == 2 || state == 3 || state == 4) ? 1 : 0;
}

/*
 * CLoopingSoundObject_Construct (FUN_00448f30)
 *
 * Creates a looping/ambient sound object (train, rain, etc.).
 * Sets sfxType=8, allocates a filename buffer, copies the default empty
 * filename, stores ResourceReplayDelay at offset +100.
 *
 * LINUX: Mix_PlayChannel(-1, chunk, -1) for infinite loop
 *        OR alSourcei(alSrc, AL_LOOPING, AL_TRUE); alSourcePlay(alSrc);
 */
void *__thiscall CLoopingSoundObject_Construct(void      *this,
                                               int        fileNameLen,
                                               int        ownerPtr,
                                               int        mapPtr,
                                               uint32_t   replayDelay,
                                               uint16_t   panValue)
{
    /* Call base one-shot constructor */
    FUN_0040cfa0(this, ownerPtr, mapPtr, panValue);

    *(int      *)((char *)this + 0x5c)  = fileNameLen;
    *(void    **)this = &vtable_CLoopingSoundObject;   /* PTR_FUN_00478280 */
    *(uint32_t *)((char *)this + 4)     = 8;           /* sfxType = looping */

    /* Allocate filename buffer */
    char *pBuf = (char *)FUN_00465ce0(fileNameLen + 1);
    *(char **)((char *)this + 0x60) = pBuf;
    if (pBuf)
        memcpy(pBuf, &DAT_004851d0, 1);   /* copy empty default filename */

    *(uint8_t  *)((char *)this + 0x58) = 0;
    *(uint32_t *)((char *)this + 100)  = replayDelay;
    return this;
}

/*
 * CTileObject_AttachSound (FUN_004546d0)
 *
 * Attaches a sound effect to a tile object.
 * If loopSource==0, allocates a one-shot sound (0x58 bytes, FUN_0040cfa0).
 * If loopSource!=0, allocates a looping sound (0x68 bytes, FUN_00448f30).
 * Updates the global sound-slot linked list (DAT_00485270).
 *
 * WIN32 (indirect): DirectSoundCreate chain
 * LINUX: Mix_LoadWAV + Mix_PlayChannel  OR  alGenBuffers + alSourcePlay
 */
void *__thiscall CTileObject_AttachSound(void    *this,
                                         int      soundId,
                                         uint16_t panValue,
                                         int      loopSource)
{
    void *pSound;

    if (soundId == 0) return DAT_00485270;
    if (!FUN_004255f0(soundId)) return DAT_00485270;

    if (*(int *)((char *)this + 0xd0) != 0) {
        /* Object already has a sound slot -- add to chain */
        if (loopSource == 0) {
            pSound = FUN_00465ce0(0x58);
            if (pSound)
                pSound = FUN_0040cfa0(pSound, (int)this, soundId, panValue);
        } else {
            pSound = FUN_00465ce0(0x68);
            if (pSound)
                pSound = FUN_00448f30(pSound, loopSource, (int)this, soundId,
                                     DAT_004855f0, panValue);
        }
        DAT_00485270[10] = pSound;
        return (void *)DAT_00485270[10];
    }

    /* First sound for this object */
    if (loopSource == 0) {
        pSound = FUN_00465ce0(0x58);
        if (pSound)
            DAT_00485270 = FUN_0040cfa0(pSound, (int)this, soundId, panValue);
        else
            DAT_00485270 = NULL;
    } else {
        pSound = FUN_00465ce0(0x68);
        if (pSound)
            DAT_00485270 = FUN_00448f30(pSound, loopSource, (int)this, soundId,
                                        DAT_004855f0, panValue);
        else
            DAT_00485270 = NULL;
    }
    *(void **)((char *)this + 0xd0) = DAT_00485270;
    return DAT_00485270;
}

/* =========================================================================
 * Section 7: Sound Handle (CSoundHandle) -- per-object sound ownership
 * ========================================================================= */

/*
 * CSoundHandle_Construct (FUN_00454b50)
 *
 * Lightweight constructor: sets vtable, zeros play position, stores
 * the sound resource ID at offset +0x1c.
 */
void __thiscall CSoundHandle_Construct(void *this, uint32_t soundResourceId)
{
    *(void    **)this                    = &vtable_CSoundHandle;
    *(uint32_t *)((char *)this + 0x14)  = 0;   /* pEffect = NULL      */
    *(uint32_t *)((char *)this + 0x18)  = 0;   /* playPosition = 0    */
    *(uint32_t *)((char *)this + 0x1c)  = soundResourceId;
    *(uint32_t *)((char *)this + 0x20)  = 0;
}

/*
 * CSoundHandle_LoadAndPlay (FUN_00454bf0)
 *
 * Resolves the stored soundResourceId against the global sound table, then
 * calls vtable[1] (GetBuffer/Play) on the resulting CSoundEffect.
 * Returns true if a sound buffer was successfully acquired.
 *
 * WIN32 (indirect): DirectSoundCreate, IDirectSoundBuffer::Play
 * LINUX: alSourcePlay (OpenAL) or Mix_PlayChannel (SDL2_mixer)
 */
int __fastcall CSoundHandle_LoadAndPlay(int pHandle)
{
    int *pEffect = (int *)CSoundTable_LoadSoundById(&DAT_004855e8,
                                                    *(int *)(pHandle + 0x1c));
    *(int **)(pHandle + 0x14) = pEffect;
    if (pEffect == NULL) return 0;

    int result = (*(int (**)(int,int))(pEffect[0] + 4))(0, 0);  /* vtable[1] = GetBuffer */
    *(int *)(pHandle + 0x18) = result;
    return result != 0;
}

/*
 * CSoundHandle_Stop (FUN_00454bc0)
 *
 * Stops the underlying DirectSound buffer without freeing memory.
 *
 * WIN32 (indirect): IDirectSoundBuffer::Stop
 * LINUX: alSourceStop (OpenAL) or Mix_HaltChannel
 */
void __fastcall CSoundHandle_Stop(int pHandle)
{
    int *pEffect = *(int **)(pHandle + 0x14);
    if (pEffect != NULL && pEffect[4] != 0) {
        (*(void (**)())(pEffect[0] + 8))();   /* vtable[2] = Stop */
    }
    *(uint32_t *)(pHandle + 0x14) = 0;
    *(uint32_t *)(pHandle + 0x18) = 0;
}

/*
 * CSoundHandle_Destruct (FUN_00454b70)
 *
 * Full destructor: stops the sound (via vtable[2]) and optionally frees memory.
 *
 * WIN32 (indirect): IDirectSoundBuffer::Stop
 * LINUX: alSourceStop + alDeleteSources + alDeleteBuffers (OpenAL)
 */
void *__thiscall CSoundHandle_Destruct(void *this, BYTE freeMemory)
{
    int *pEffect = *(int **)((char *)this + 0x14);
    *(void **)this = &vtable_CSoundHandle;
    if (pEffect != NULL && pEffect[4] != 0) {
        (*(void (**)())(pEffect[0] + 8))();   /* vtable[2] = Stop */
    }
    *(uint32_t *)((char *)this + 0x14) = 0;
    *(uint32_t *)((char *)this + 0x18) = 0;
    if (freeMemory & 1)
        FUN_00465cd0(this);
    return this;
}

/* =========================================================================
 * Section 8: Game World sound cleanup / initialisation
 * ========================================================================= */

/*
 * CGameWorld_Construct (FUN_00454cf0)
 *
 * Allocates two ambient sound channel objects via FUN_0045cdf0:
 *   slot 7 (rain/ambient) -> pWorld+0x14922
 *   slot 8 (train/music)  -> pWorld+0x14923
 * These use the DirectSound chain for looping ambient audio.
 * Also resets the tile map and clears all sound-handle pointers.
 *
 * WIN32 (indirect): DirectSoundCreate, IDirectSoundBuffer::Play (looping)
 * LINUX: alGenSources x2 + alSourcei(AL_LOOPING, AL_TRUE)
 *        OR Mix_PlayChannel(-1, chunk, -1)
 */
void *__fastcall CGameWorld_Construct(uint32_t *pWorld)
{
    void *pSoundObj;

    /* Set vtable, reset tile map */
    *pWorld = (uint32_t)&vtable_CGameWorld;
    CGameWorld_Reset((int)pWorld);  /* FUN_00454fe0 */
    pWorld[7] = 0;
    pWorld[8] = 0;

    /* Allocate ambient sound channel 0 (slot 7 = rain/ambient) */
    pSoundObj = FUN_00465ce0(0x2c);
    if (pSoundObj)
        pWorld[0x14922] = (uint32_t)FUN_0045cdf0(pSoundObj, 7);
    else
        pWorld[0x14922] = 0;

    /* Allocate ambient sound channel 1 (slot 8 = train/music) */
    pSoundObj = FUN_00465ce0(0x2c);
    if (pSoundObj)
        pWorld[0x14923] = (uint32_t)FUN_0045cdf0(pSoundObj, 8);
    else
        pWorld[0x14923] = 0;

    pWorld[0x14921] = 0;
    *(uint8_t *)((char *)pWorld + 0x3c) = 0;
    *(uint8_t *)((char *)pWorld + 0x52510) = 0;
    memset(pWorld + 0x14925, 0, 0x1f * sizeof(uint32_t));
    return pWorld;
}

/*
 * CGameWorld_Cleanup (FUN_00454de0)
 *
 * Destroys both ambient sound channels and the tile-visibility bitmap.
 *
 * WIN32 (indirect): IDirectSoundBuffer::Stop, IDirectSoundBuffer::Release
 * LINUX: alDeleteSources + alDeleteBuffers (OpenAL) or Mix_FreeChunk
 */
void __fastcall CGameWorld_Cleanup(int pWorld)
{
    uint32_t *pSnd;

    /* Destroy ambient sound channel 0 */
    pSnd = *(uint32_t **)(pWorld + 0x52488);
    if (pSnd) {
        FUN_0045ce10(pSnd);     /* stop + release DirectSound buffer */
        FUN_00465cd0(pSnd);     /* operator delete                   */
        *(uint32_t *)(pWorld + 0x52488) = 0;
    }

    /* Destroy ambient sound channel 1 */
    pSnd = *(uint32_t **)(pWorld + 0x5248c);
    if (pSnd) {
        FUN_0045ce10(pSnd);
        FUN_00465cd0(pSnd);
        *(uint32_t *)(pWorld + 0x5248c) = 0;
    }

    /* Free tile-visibility bitmap */
    if (*(uint8_t **)(pWorld + 0x52484)) {
        FUN_00465cd0(*(uint8_t **)(pWorld + 0x52484));
        *(uint32_t *)(pWorld + 0x52484) = 0;
    }
}

/*
 * CGameWorld_Reset (FUN_00454fe0)
 *
 * Zeros the entire tile-object pointer grid (0x14910 x uint32_t entries),
 * resets the visibility bitmap to 0xFF (all visible), clears per-tile
 * layer-count bytes, and forces an immediate window repaint.
 *
 * WIN32: InvalidateRect, UpdateWindow
 * LINUX: SDL_RenderPresent / signal dirty-region queue
 */
void __fastcall CGameWorld_Reset(int pWorld)
{
    uint32_t *pTiles = (uint32_t *)(pWorld + 0x44);

    /* Clear all tile slots */
    memset(pTiles, 0, 0x14910 * sizeof(uint32_t));

    /* Reset visibility bitmap (all bits set = all tiles visible) */
    uint8_t **ppBitmap = (uint8_t **)(pWorld + 0x52484);
    if (*ppBitmap) {
        int nTilesW = *(int16_t *)(pWorld + 0x3e);
        int nTilesH = *(int16_t *)(pWorld + 0x40);
        int bitmapBytes = ((nTilesW * nTilesH + 7) >> 3) + 1;
        memset(*ppBitmap, 0xFF, bitmapBytes);
    }

    /* Force window repaint if main window is active */
    if (DAT_004aa4a0 != 0 &&
        *(HWND *)(DAT_004aa4a0 + 8) != NULL &&
        DAT_004851f4 != 1)
    {
        /* WIN32: */
        InvalidateRect(*(HWND *)(DAT_004aa4a0 + 8), NULL, FALSE);
        UpdateWindow (*(HWND *)(DAT_004aa4a0 + 8));
        /* LINUX:
           SDL_RenderPresent(renderer);
        */
    }
}

/*
 * CGameWorld_SetResolution (FUN_00454e60)
 *
 * Configures the tile-grid dimensions from screen resolution.
 * Defaults to 1024x768 if screen width is below 0x400 or above 0x500.
 * Allocates/reallocates the per-tile visibility bitmap.
 *
 * LINUX: SDL_GetCurrentDisplayMode(0, &mode) for screen dimensions
 */
void __thiscall CGameWorld_SetResolution(void *this, char isHighRes)
{
    int w, h;

    if (!isHighRes && DAT_004851d8 > 0x3FF && DAT_004851d8 < 0x501) {
        w = DAT_004851d8;
        h = DAT_00485214;
    } else if (!isHighRes) {
        w = 0x400; h = 0x300;   /* 1024x768 default */
    } else {
        if (DAT_004851d8 > 0x3FF && DAT_004851d8 < 0x501) {
            w = DAT_004851d8; h = DAT_00485214;
        } else {
            w = 0x500; h = 0x400;
        }
    }

    *(int16_t *)((char *)this + 0x3e) = (int16_t)((w + 15) >> 4);  /* tiles wide */
    *(int16_t *)((char *)this + 0x40) = (int16_t)((h + 15) >> 4);  /* tiles tall */

    /* Reallocate visibility bitmap */
    uint8_t **ppBitmap = (uint8_t **)((char *)this + 0x52484);
    if (*ppBitmap) {
        FUN_00465cd0(*ppBitmap);
        *ppBitmap = NULL;
    }
    int nTiles    = *(int16_t *)((char *)this + 0x3e)
                  * *(int16_t *)((char *)this + 0x40);
    int bitmapSz  = ((nTiles + 7) >> 3) + 1;
    *ppBitmap = (uint8_t *)FUN_00465ce0(bitmapSz);
    if (*ppBitmap)
        memset(*ppBitmap, 0xFF, bitmapSz);
}

/* =========================================================================
 * Section 9: MCI_SETAUDIO command value reference
 *
 *   mciSendCommandA(mciId, MCI_SETAUDIO=0x806, flags, &params)
 *   flags used: 0x1000001 = MCI_NOTIFY(0x1) | MCI_SETAUDIO_VOLUME(0x1000000)
 *   This sets the audio volume of the MCI device.
 *
 *   The audio volume is stored at this+0x18 in the CMciVideoPlayer object.
 *   Default value observed: uVar1 & 0xFFFF  (lower 16 bits from parent HWND field)
 * ========================================================================= */

/* =========================================================================
 * DSOUND.DLL Import thunks (Ordinal_1 and Ordinal_2)
 *
 * Both are IAT thunks. Ghidra could not disassemble the jump-table bodies
 * due to indirect branching.  Their signatures are:
 *
 *   Ordinal_1 = DirectSoundCreate(LPGUID, LPDIRECTSOUND*, LPUNKNOWN)
 *             -> creates the IDirectSound COM object.
 *             -> LINUX: alcOpenDevice + alcCreateContext (OpenAL)
 *             -> OR:    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)
 *
 *   Ordinal_2 = DirectSoundEnumerate(LPDSENUMCALLBACK, LPVOID)
 *             -> enumerates audio output devices.
 *             -> LINUX: alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER)
 *             -> OR:    SDL_GetNumAudioDevices + SDL_GetAudioDeviceName
 * ========================================================================= */

/* end of network.c (loco_audio.c) */
