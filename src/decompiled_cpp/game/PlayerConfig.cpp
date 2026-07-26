/**
 * PlayerConfig.cpp — PlayerConfig implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file implements the PlayerConfig class (per-player settings) and the
 * PlayerRecord_constructor function that initializes a PlayerConfig struct
 * from INI/config settings (reading the last logged-in player name from
 * LOCO.INI [Configuration] section).
 */

#include "PlayerConfig.h"
#include "../shared/vtable_addrs.h"
#include "../shared/types.h"

#include <string.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

#ifdef _WIN32
extern "C" {
    /* Windows API via IAT */
    DWORD __stdcall GetUserNameA(char* buffer, DWORD* size); /* 0x477014 */
    HANDLE __stdcall CreateFileA(const char* path, DWORD access, DWORD share,
                                 void* security, DWORD creation, DWORD flags,
                                 HANDLE template_file);  /* 0x4770B4 */
    BOOL __stdcall ReadFile(HANDLE file, void* buffer, DWORD bytes,
                            DWORD* read_bytes, void* overlapped);   /* 0x4770BC */
    BOOL __stdcall WriteFile(HANDLE file, const void* buffer, DWORD bytes,
                             DWORD* written, void* overlapped);     /* 0x4770A4 */
    BOOL __stdcall CloseHandle(HANDLE object);                      /* 0x4770A0 */
    int __cdecl wsprintfA(char* out, const char* fmt, ...);         /* 0x477370 */
}
#endif
#ifndef _WIN32
/* POSIX equivalents — implemented in shared/link_stubs.cpp */
#define GENERIC_READ  0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define INVALID_HANDLE_VALUE ((HANDLE)(uintptr_t)-1)

extern "C" {
    HANDLE CreateFileA(const char*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
    BOOL   ReadFile(HANDLE, void*, DWORD, DWORD*, void*);
    BOOL   WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
    BOOL   CloseHandle(HANDLE);
    int    wsprintfA(char*, const char*, ...);
    DWORD  GetUserNameA(char*, DWORD*);
}
#endif


/* Game helpers — C++ linkage (called from decompiled code) */
UINT Config_GetIniInt(void* ini, const char* section, const char* key, int default_val);
void Config_GetIniString(void* ini, const char* section, const char* key,
                         const char* default_str, char* buffer, DWORD bufSize);
void Config_WriteInt(void* ini, const char* section, const char* key, int value);

/* Internal helpers (C++ linkage) */
void LOCOBITMAP_ColorKeyBlit_thunk(void* dplay_cfg);
void INPUT_InitNetworkPlayer(void* cursor);
void NET_UpdatePlayerList(void);

/* Global variables */
extern char g_install_path[];               /* 0x4A99C8 — install directory path */
extern void* g_config_ini;                  /* 0x485484 — config INI object handle */
extern void* g_cursor;                      /* 0x4FD380 — cursor/input state */
extern PlayerConfig* g_player_config;       /* 0x4AA4A8 — global player config ptr */
extern void* g_dplay_config;                /* 0x4FD3B4 — DirectPlay config ptr */

/* String constants */
#define STR_CLIENT_SECTION  "CLIENT"     /* 0x47F08C */
#define STR_NEXTID_KEY      "NextId"     /* 0x47F094 */
#define STR_PATH_FORMAT     "%s\\%s\\usr" /* 0x47F09C — {install_path}\{name}\usr */
#define STR_SAVEDATA_FORMAT "%03d_%04d"  /* 0x47F0A8 — save-ID string format */

/**
 * String constants used by PlayerRecord_constructor:
 *   s_Configuration_0047e734 = "Configuration"  (INI section)
 *   s_PlayerName_0047e73c   = "PlayerName"     (INI key)
 *   g_empty_string (0x4851D0)                  = "" (empty default string)
 *   s_LEGO_LOCO_0047e1c0   = "LEGO LOCO"       (fallback player name)
 */
extern const char s_Configuration_0047e734[];  /* "Configuration" at 0x47E734 */
extern const char s_PlayerName_0047e73c[];     /* "PlayerName" at 0x47E73C */
extern const char g_empty_string[];             /* "" at 0x4851D0 */
extern const char s_LEGO_LOCO_0047e1c0[];       /* "LEGO LOCO" at 0x47E1C0 */

/* ================================================================== */
/* PlayerConfig::PlayerConfig                                          */
/* Address: 0x452CE0                                                   */
/*                                                                     */
/* Copies the player name string into the struct starting at +0x04     */
/* (which overlay the magic + name field range). Sets the descriptor   */
/* at +0x00 and zeroes +0x108.                                         */
/*                                                                     */
/* Called by: g_player_config allocation in GameLoop_Setup             */
/* ================================================================== */
PlayerConfig::PlayerConfig(const char* name)
{
    size_t len;
    const char* src;
    char* dst;

    this->descriptor = (void*)VTBL_PLAYERCONFIG;   /* +0x00 */

    /* Copy name string (including null terminator) to +0x04 */
    /* This writes the name into the overlapping magic+name area */
    dst = (char*)&this->magic;                     /* +0x04 */
    src = name;
    len = strlen(name) + 1;

    /* Word-aligned copy (REP MOVSD for bulk, REP MOVSB for remainder) */
    /* Simplified: */
    memcpy(dst, src, len);

    this->field_108 = 0;                            /* +0x108 */
}

/* ================================================================== */
/* PlayerConfig::~PlayerConfig                                         */
/* Address: 0x452D50                                                   */
/*                                                                     */
/* Resets descriptor to VTBL_PLAYERCONFIG. Minimal cleanup.            */
/* ================================================================== */
PlayerConfig::~PlayerConfig()
{
    this->descriptor = (void*)VTBL_PLAYERCONFIG;   /* +0x00 */
}

/* ================================================================== */
/* PlayerConfig::LoadOrCreate                                          */
/* Address: 0x4530C0                                                   */
/*                                                                     */
/* Called by: PlayerConfig::SetName (0x453030),                        */
/*            PlayerRecord_constructor (0x452F10)                      */
/*                                                                     */
/* Flow:                                                               */
/*   1. Save name from +0x06 to local buffer, write magic at +0x04    */
/*   2. Build file path: g_install_path\\name\\usr                     */
/*   3. Try OPEN_EXISTING + GENERIC_READ                               */
/*   4. If file doesn't exist: return 0 (caller creates new)          */
/*   5. Read 288 bytes into offset +0x04                               */
/*   6. Check magic; if valid: return 1                                */
/*   7. If magic invalid: get NextId from INI, create fresh file      */
/*      with new ID, return 1                                         */
/* ================================================================== */
int PlayerConfig::LoadOrCreate()
{
    char name_copy[16];      /* local_a20 — name backup */
    char file_path[1284];    /* local_a08 — path to existing file */
    char new_file_path[1284]; /* local_504 — path for new file */
    HANDLE hFile;
    DWORD bytes_read;
    DWORD bytes_written;
    BOOL success;
    int next_id;

    /* Step 1: Save name, write magic, clear fields */
    strcpy(name_copy, this->name);              /* save name from +0x06 */

    this->magic = PLAYERCONFIG_MAGIC;           /* +0x04 = 0x66 */
    this->name[0] = '\0';                       /* +0x06[0] = 0 */
    this->field_14 = 0;                         /* +0x14 = 0 */
    this->player_id = 0;                        /* +0x18 = 0 */
    this->save_counter = 0;                     /* +0x1C = 0 */
    strcpy(this->name, name_copy);              /* restore name at +0x06 */

    /* Step 2: Build file path */
    wsprintfA(file_path, STR_PATH_FORMAT, g_install_path, name_copy);

    /* Step 3: Try to open existing file */
    hFile = CreateFileA(file_path,
                        GENERIC_READ,           /* 0x80000000 */
                        FILE_SHARE_READ,        /* 1 */
                        NULL,                   /* no security */
                        OPEN_EXISTING,          /* 3 */
                        FILE_FLAG_NO_BUFFERING, /* 0x8000000 */
                        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;   /* File doesn't exist — caller handles new player creation */
    }

    /* Step 5: Read config data from disk */
    success = ReadFile(hFile, &this->magic, PLAYERCONFIG_FILE_SIZE,
                       &bytes_read, NULL);

    if (!success) {
        CloseHandle(hFile);    /* read failed */
        return 0;
    }
    CloseHandle(hFile);

    /* Step 6: Check magic */
    if (this->magic == PLAYERCONFIG_MAGIC) {
        return 1;   /* Successfully loaded existing config */
    }

    /* Step 7: Magic mismatch — create new config file */
    this->magic = PLAYERCONFIG_MAGIC;
    this->name[0] = '\0';
    this->field_14 = 0;
    this->player_id = 0;
    this->save_counter = 0;
    strcpy(this->name, name_copy);

    /* Get next available player ID from INI config */
    next_id = Config_GetIniInt(g_config_ini,
                               STR_CLIENT_SECTION,
                               STR_NEXTID_KEY,
                               0);

    if (next_id > PLAYERCONFIG_MAX_ID) {
        next_id = 1;
    }

    Config_WriteInt(g_config_ini, STR_CLIENT_SECTION,
                    STR_NEXTID_KEY, next_id + 1);

    this->player_id = next_id;  /* +0x18 */

    /* Write fresh config to disk */
    wsprintfA(new_file_path, STR_PATH_FORMAT, g_install_path, this->name);

    hFile = CreateFileA(new_file_path,
                        GENERIC_WRITE,          /* 0x40000000 */
                        FILE_SHARE_READ,        /* 1 */
                        NULL,
                        CREATE_ALWAYS,          /* 2 */
                        FILE_FLAG_NO_BUFFERING, /* 0x8000000 */
                        NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, &this->magic, PLAYERCONFIG_FILE_SIZE,
                  &bytes_written, NULL);
        CloseHandle(hFile);
    }

    return 1;
}

/* ================================================================== */
/* PlayerConfig::Save                                                  */
/* Address: 0x4532A0                                                   */
/*                                                                     */
/* Called by: Train_ProcessMessages (0x439A49),                        */
/*            UI_Panel_HitTest (0x4226DC)                              */
/*                                                                     */
/* Writes 288 bytes from +0x04 to "{install_path}\{name}\usr".        */
/* Silently ignores file I/O errors.                                   */
/* ================================================================== */
void PlayerConfig::Save()
{
    char file_path[1284];   /* local_504 */
    HANDLE hFile;
    DWORD bytes_written;

    wsprintfA(file_path, STR_PATH_FORMAT, g_install_path, this->name);

    hFile = CreateFileA(file_path,
                        GENERIC_WRITE,          /* 0x40000000 */
                        FILE_SHARE_READ,        /* 1 */
                        NULL,
                        CREATE_ALWAYS,          /* 2 */
                        FILE_FLAG_NO_BUFFERING, /* 0x8000000 */
                        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return;  /* Silent failure */
    }

    WriteFile(hFile, &this->magic, PLAYERCONFIG_FILE_SIZE,
              &bytes_written, NULL);
    CloseHandle(hFile);
}

/* ================================================================== */
/* PlayerConfig::SaveToFile                                            */
/* Address: 0x453320                                                   */
/*                                                                     */
/* Formats a save-ID string at +0x20 from player_id and               */
/* save_counter ("%03d_%04d", e.g. "001_0000"), increments the        */
/* save counter (wraps at 10000 to 0), writes 288 bytes to disk,      */
/* and returns pointer to the formatted string at +0x20.              */
/*                                                                     */
/* @return  const char* — pointer to the save-ID string at +0x20      */
/* ================================================================== */
const char* PlayerConfig::SaveToFile()
{
    char file_path[1284];   /* local_504 */
    HANDLE hFile;
    DWORD bytes_written;

    /* Format save-ID string at +0x20 */
    wsprintfA((char*)(this->reserved),           /* +0x20 */
              STR_SAVEDATA_FORMAT,
              this->player_id,                   /* +0x18 */
              this->save_counter);               /* +0x1C */

    /* Increment save counter, wrap at 10000 */
    this->save_counter++;
    if (this->save_counter > PLAYERCONFIG_SAVEID_MAX) {
        this->save_counter = 0;
    }

    /* Write to disk */
    wsprintfA(file_path, STR_PATH_FORMAT, g_install_path, this->name);

    hFile = CreateFileA(file_path,
                        GENERIC_WRITE,          /* 0x40000000 */
                        FILE_SHARE_READ,        /* 1 */
                        NULL,
                        CREATE_ALWAYS,          /* 2 */
                        FILE_FLAG_NO_BUFFERING, /* 0x8000000 */
                        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return (const char*)(this->reserved);  /* Return string even on write failure */
    }

    WriteFile(hFile, &this->magic, PLAYERCONFIG_FILE_SIZE,
              &bytes_written, NULL);
    CloseHandle(hFile);

    return (const char*)(this->reserved);
}

/* ================================================================== */
/* PlayerConfig::SetName                                               */
/* Address: 0x452FC0                                                   */
/*                                                                     */
/* Called by: UI_Panel_HitTest (0x4226D1) when editing player name     */
/*                                                                     */
/* Compares the current player name at +0x06 with the new name.        */
/* If different:                                                       */
/*   1. Copies the new name to +0x06                                   */
/*   2. Optionally flushes DPlay config via LOCOBITMAP_ColorKeyBlit    */
/*   3. Calls LoadOrCreate()                                           */
/*   4. If LoadOrCreate returns 0 (new player):                        */
/*      a. Gets NextId from CLIENT/NextId INI key, wraps at 999       */
/*      b. Writes NextId+1 back to INI                                 */
/*      c. Sets player_id (+0x18) to the assigned ID                   */
/*      d. Sets is_new_player (+0x120) to 1                            */
/*   5. If LoadOrCreate returns 1 (existing player):                   */
/*      Sets is_new_player (+0x120) to 0                               */
/*   6. If g_player_config is non-NULL:                                */
/*      Calls INPUT_InitNetworkPlayer(g_cursor) and                    */
/*      NET_UpdatePlayerList() to reinitialize network state.          */
/*                                                                     */
/* If names are equal: no-op (returns immediately).                     */
/*                                                                     */
/* @param new_name  const char* — new player name string               */
/* ================================================================== */
void PlayerConfig::SetName(const char* new_name)
{
    size_t name_len;
    char* dst;
    const char* src;
    int next_id;
    int load_result;

    /* Compare current name (+0x06) with new name. Uses string comparison
       that also handles the magic at +0x04 being part of the comparison
       range. In practice, the magic word is always 0x66 and the name
       follows at +0x06, so this effectively compares player names. */
    if (strcmp(this->name, new_name) == 0) {
        return;  /* Names match — no change needed */
    }

    /* Step 1: Copy new name to +0x06 */
    /* The original uses a manual word-aligned copy (REP MOVSD/MOVSB)
       instead of strcpy — simplified here to memcpy. */
    name_len = strlen(new_name) + 1;
    memcpy(this->name, new_name, name_len);

    /* Step 2: Optionally flush DPlay config if present */
    if (g_dplay_config != NULL) {
        LOCOBITMAP_ColorKeyBlit_thunk(g_dplay_config);  /* 0x401680 */
    }

    /* Step 3: Load or create player config from disk */
    load_result = this->LoadOrCreate();

    if (load_result == 0) {
        /* New player — assign a unique ID */
        next_id = Config_GetIniInt(g_config_ini,
                                    STR_CLIENT_SECTION,
                                    STR_NEXTID_KEY,
                                    0);

        if (next_id > PLAYERCONFIG_MAX_ID) {
            next_id = 1;
        }

        Config_WriteInt(g_config_ini, STR_CLIENT_SECTION,
                        STR_NEXTID_KEY, next_id + 1);

        this->player_id = next_id;      /* +0x18 */
        this->is_new_player = 1;        /* +0x120 */
    } else {
        /* Existing player loaded from disk */
        this->is_new_player = 0;        /* +0x120 */
    }

    /* Step 4: Reinitialize network player list */
    if (g_player_config != NULL) {
        INPUT_InitNetworkPlayer(g_cursor);     /* 0x41A0E0 */
        NET_UpdatePlayerList();                 /* 0x445170 */
    }
}

/* ================================================================== */
/* PlayerRecord_constructor — Initialize config from INI settings      */
/* Address: 0x452E10                                                   */
/* Calling convention: __fastcall (ECX = param_1 = PlayerConfig*)     */
/*                                                                     */
/* Called by: GameLoop_Setup (0x406CED)                                */
/*                                                                     */
/* This function initializes a PlayerConfig struct from the LOCO.INI   */
/* [Configuration] settings. It:                                        */
/*                                                                     */
/*   1. Sets descriptor at +0x00 to VTBL_PLAYERRECORD (0x4784C0)      */
/*   2. Sets magic at +0x04 to 0x66, name at +0x06 to empty           */
/*   3. Clears field_14, player_id, save_counter, is_new_player       */
/*   4. Reads the last player name from INI [Configuration] section    */
/*      (key "PlayerName"). If empty, tries GetUserNameA().            */
/*      If still empty, falls back to "LEGO LOCO".                     */
/*   5. Compares the INI-derived name with the name currently at       */
/*      +0x06. If different (or currently empty):                    */
/*      a. Copies the INI name to +0x06                                */
/*      b. Optionally flushes DPlay config                             */
/*      c. Calls LoadOrCreate() to persist the config                  */
/*      d. If new player: assigns ID from CLIENT/NextId                */
/*      e. If existing: clears is_new_player                           */
/*      f. Reinitializes network player list                           */
/*   6. Returns param_1 (the PlayerConfig pointer).                    */
/*                                                                     */
/* This is called once during startup to ensure the player config      */
/* struct is populated before the main menu appears. It restores the   */
/* last logged-in player's settings from the INI file.                 */
/*                                                                     */
/* @param config  ECX = pointer to PlayerConfig struct to initialize.  */
/* @return        The config pointer (param_1).                        */
/* ================================================================== */
PlayerConfig* PlayerRecord_constructor(PlayerConfig* config)
{
    char name_buf[16];      /* local — INI name buffer */
    DWORD buf_size;
    const char* ini_name;
    size_t name_len;

    /* Step 1: Initialize struct fields */
    config->descriptor = (void*)VTBL_PLAYERRECORD;  /* +0x00 = 0x4784C0 */
    config->magic = PLAYERCONFIG_MAGIC;             /* +0x04 = 0x66 */
    config->name[0] = '\0';                         /* +0x06[0] = 0 */
    config->field_14 = 0;                           /* +0x14 = 0 */
    config->player_id = 0;                          /* +0x18 = 0 */
    config->save_counter = 0;                       /* +0x1C = 0 */
    config->is_new_player = 0;                      /* +0x120 = 0 */

    /* Step 2: Read player name from INI [Configuration] section */
    name_buf[0] = '\0';
    buf_size = 13;  /* 0xD = 13 bytes max */

    Config_GetIniString(g_config_ini,
                        s_Configuration_0047e734,   /* "Configuration" */
                        s_PlayerName_0047e73c,      /* "PlayerName" */
                        g_empty_string,              /* "" default */
                        name_buf,
                        buf_size);

    /* Step 3: If INI name is empty, try Windows user name */
    if (name_buf[0] == '\0') {
        DWORD name_len_w = buf_size;
        GetUserNameA(name_buf, &name_len_w);

        /* If still empty, use hardcoded fallback "LEGO LOCO" */
        if (name_buf[0] == '\0') {
            strcpy(name_buf, s_LEGO_LOCO_0047e1c0);  /* "LEGO LOCO" */
        }
    }

    /* Step 4: Compare with existing name at +0x06 */
    if (strcmp(config->name, name_buf) != 0) {
        /* Names differ — copy the INI-resolved name */
        memcpy(config->name, name_buf, strlen(name_buf) + 1);

        /* Optionally flush DPlay config if present */
        if (g_dplay_config != NULL) {
            LOCOBITMAP_ColorKeyBlit_thunk(g_dplay_config);
        }

        /* Load or create player config */
        int result = config->LoadOrCreate();

        if (result == 0) {
            /* New player — assign unique ID from CLIENT/NextId */
            int next_id = Config_GetIniInt(g_config_ini,
                                            STR_CLIENT_SECTION,
                                            STR_NEXTID_KEY,
                                            0);

            if (next_id > PLAYERCONFIG_MAX_ID) {
                next_id = 1;
            }

            Config_WriteInt(g_config_ini, STR_CLIENT_SECTION,
                            STR_NEXTID_KEY, next_id + 1);

            config->player_id = next_id;        /* +0x18 */
            config->is_new_player = 1;          /* +0x120 */
        } else {
            /* Existing player loaded from disk */
            config->is_new_player = 0;          /* +0x120 */
        }

        /* Reinitialize network player list if global config exists */
        if (g_player_config != NULL) {
            INPUT_InitNetworkPlayer(g_cursor);     /* 0x41A0E0 */
            NET_UpdatePlayerList();                 /* 0x445170 */
        }
    }

    return config;
}


/* ================================================================== */
/* PlayerConfig_Ctor — Legacy bridge for decompiled callers            */
/*                                                                     */
/* CGWND_InstallPathInit and Game.cpp call PlayerConfig_Ctor(void*,    */
/* const char*) as a free function (Ghidra decompiler artifact).       */
/* This bridge wraps the proper C++ constructor.                       */
/* ================================================================== */
#include <new>
void* PlayerConfig_Ctor(void* mem, const char* path)
{
    if (!mem) return nullptr;
    return new (mem) PlayerConfig(path);
}
