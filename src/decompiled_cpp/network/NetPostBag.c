/**
 * NetPostBag.c — PostBag file-IO network subsystem
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains the PostBag-based file I/O network layer. Lego Loco's
 * "multiplayer" system actually works by dropping .crd and .dat files into
 * shared PostBag directories (Sort_In, Sort_Out, Att_Out, Att_In, Album,
 * Sort_Bag). The NET_* functions here manage file path construction,
 * enumeration, read/write, and cleanup for these directories.
 *
 * === PostBag Subdirectories (type codes) ===
 *   0 = Album        — postcard album storage
 *   1 = Sort_In      — incoming sorted mail
 *   2 = Sort_Out     — outgoing sorted mail
 *   3 = Sort_Bag     — mail bag storage
 *   4 = Att_Out      — outgoing attachments
 *   5 = Att_In       — incoming attachments
 *   6 = Easter       — easter-egg content (language-dependent)
 *   7 = Design       — custom level designs
 *
 * === File Naming Conventions ===
 *   .crd files:  PostBag\<subdir>\<name>_<configId>.crd  (player data)
 *   .crd files:  PostBag\<subdir>\<name>_<num>_<configId>.crd
 *   .crd files:  PostBag\<subdir>\_<id>_<configId>.crd  (readback)
 *   .dat files:  PostBag\<subdir>\<id>.dat  (attachment data, 0x400 bytes)
 *   .att files:  PostBag\<subdir>\<id>.att  (attachment metadata)
 *
 * === File format ===
 *   All .crd files have a 2-byte magic at offset 0: 0x66 = valid player data.
 *   .dat files are raw 0x400-byte attachment payloads.
 *
 * === Global context ===
 *   g_dplay (0x4FD3B0) — NetworkPlayerList instance
 *   g_dplay_config (0x4FD3A4) — DPLAY config instance (a DLayoutConfig)
 *   g_install_path (0x4A99C8) — game install root
 *   g_config_ini (0x4A9EEC) — config INI file handle
 *   DAT_004a97a0 — language code (0=Eng, 1=Dan, 2=Dut, 4=Fre, 5=Ger,
 *                  6=Ita, 7=Nor, 8=Spa, 9=Swe)
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

/* Win32 API */
extern void*   __stdcall CreateFileA(const char*, uint32_t, uint32_t,
                                      void*, uint32_t, uint32_t, void*);
extern int32_t __stdcall ReadFile(void*, void*, uint32_t, uint32_t*, void*);
extern int32_t __stdcall WriteFile(void*, const void*, uint32_t,
                                    uint32_t*, void*);
extern int32_t __stdcall CloseHandle(void*);
extern int32_t __stdcall DeleteFileA(const char*);
extern uint32_t __stdcall SetFileAttributesA(const char*, uint32_t);
extern uint32_t __stdcall GetFileAttributesA(const char*);
extern int32_t  __stdcall CreateDirectoryA(const char*, void*);
extern int32_t  __stdcall CopyFileA(const char*, const char*, int32_t);
extern int32_t  __stdcall wsprintfA(char*, const char*, ...);

/* CRT helpers */
extern void* __cdecl operator_new(size_t size);       /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);            /* 0x465CD0 */

/* Game globals */
extern char    g_install_path[];       /* 0x4A99C8 */
extern void*   g_config_ini;           /* 0x4A9EEC */
extern void*   g_player_config;         /* 0x4AA4A8 */
extern int32_t DAT_004a97a0;           /* 0x4A97A0 — language code */

/* DPlayManager helpers */
extern void* __fastcall DPLAY_CreatePlayer(void* slot);     /* 0x442850 */
extern int32_t __thiscall DPLAY_GetPlayerName(void* slot, const char* path); /* 0x442B50 */

/* ================================================================== */
/* Static helpers — CRT FindFirst/FindNext wrappers                     */
/* ================================================================== */

extern void* __stdcall CRT_FindFirstFile(const char* lpFileName, void* lpFindFileData);
extern int32_t __stdcall CRT_FindNextFile(void* hFindFile, void* lpFindFileData);
extern int32_t __stdcall CRT_FindClose(void* hFindFile);

/* ================================================================== */
/* PostBag subdirectory name table                                      */
/* ================================================================== */

static const char* PostBag_Subdir(int32_t type)
{
    switch (type) {
    case 0: return (const char*)0x0047eba4;   /* "Album"     */
    case 1: return (const char*)0x0047ebc4;   /* "Sort_In"   */
    case 2: return (const char*)0x0047ebb8;   /* "Sort_Out"  */
    case 3: return (const char*)0x0047ebac;   /* "Sort_Bag"  */
    case 4: return (const char*)0x0047eb90;   /* "Att_Out"   */
    case 5: return (const char*)0x0047eb9c;   /* "Att_In"    */
    case 6: {  /* Easter (language-dependent) */
        switch (DAT_004a97a0) {
        default: return (const char*)0x0047ebf8;  /* "Easter_Eng" */
        case 1:  return (const char*)0x0047ec58;  /* "Easter_Dan" */
        case 2:  return (const char*)0x0047ec4c;  /* "Easter_Dut" */
        case 4:  return (const char*)0x0047ec40;  /* "Easter_Fre" */
        case 5:  return (const char*)0x0047ec34;  /* "Easter_Ger" */
        case 6:  return (const char*)0x0047ec28;  /* "Easter_Ita" */
        case 7:  return (const char*)0x0047ec1c;  /* "Easter_Nor" */
        case 8:  return (const char*)0x0047ec10;  /* "Easter_Spa" */
        case 9:  return (const char*)0x0047ec04;  /* "Easter_Swe" */
        }
    }
    case 7: return (const char*)0x0047ed18;   /* "Design"    */
    default: return (const char*)0x0047eba4;   /* fallback: "Album" */
    }
}

/* ================================================================== */
/* NET_GetHostName — 0x4446F0                                           */
/*                                                                      */
/* Enumerate .crd files in a PostBag subdirectory. Constructs two       */
/* path formats depending on param_2:                                   */
/*   param_2 == 0:  <install>\PostBag\<subdir>\<name>_<configId>.crd   */
/*   param_2 != 0:  <install>\PostBag\<subdir>\<param2>_<configId>.crd */
/*                                                                      */
/* Returns a linked list of 0x508-byte nodes (filepath at +0x00,       */
/* next pointer at +0x504). Only includes files with magic 0x66.       */
/*                                                                      */
/* Called by: DPLAY_GetMessageCount, DPLAY_EnumeratePlayers,           */
/*            NET_RegisterPlayer, NET_UnregisterPlayer,                 */
/*            NET_GetPlayerAddress, NET_UpdatePlayerList,               */
/*            Town_ReceivePostcard, Town_SavePostcard, etc.             */
/* ================================================================== */
void* __cdecl NET_GetHostName(int32_t param_1, int32_t param_2)
{
    char  path_wild[0x508];
    char  path_base[0x508];
    const char* subdir;
    void* hFind;
    void* find_data[5];  /* WIN32_FIND_DATAA */
    char  find_name[260];
    void* result_list;
    void* node;
    void* hFile;
    int16_t magic;
    uint32_t bytes_read;

    /* Initialize buffers */
    path_wild[0] = *(char*)0x4851D0;  /* g_empty_string */
    {
        uint32_t* p = (uint32_t*)(path_wild + 1);
        int32_t i;
        for (i = 0; i < 0x140; i++) p[i] = 0;
    }
    path_wild[0x501] = 0;
    path_wild[0x502] = 0;

    path_base[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(path_base + 1);
        int32_t i;
        for (i = 0; i < 0x140; i++) p[i] = 0;
    }
    path_base[0x501] = 0;
    path_base[0x502] = 0;

    result_list = NULL;
    bytes_read = 0;

    subdir = PostBag_Subdir(param_1);

    if (param_2 == 0) {
        /* Format: <install>\PostBag\<subdir>\<name>_<configId>.crd */
        wsprintfA(path_wild,
                  (const char*)0x0047ece4,  /* "%s%s%s_%03d.crd" */
                  g_install_path,
                  (const char*)0x0047e0c4,  /* "\PostBag\" */
                  subdir,
                  *(int32_t*)((int8_t*)g_player_config + 0x18));
        wsprintfA(path_base,
                  (const char*)0x0047ecdc,  /* "%s%s%s\\" */
                  g_install_path,
                  (const char*)0x0047e0c4,
                  subdir);
    } else {
        /* Format: <install>\PostBag\<subdir>\<param2>_<configId>.crd */
        wsprintfA(path_wild,
                  (const char*)0x0047ed04,  /* "%s%s%s_%s_%03d.crd" */
                  g_install_path,
                  (const char*)0x0047e0c4,
                  subdir,
                  param_2,
                  *(int32_t*)((int8_t*)g_player_config + 0x18));
        wsprintfA(path_base,
                  (const char*)0x0047ecf8,  /* "%s%s%s_%s\\" */
                  g_install_path,
                  (const char*)0x0047e0c4,
                  subdir,
                  param_2);
    }

    hFind = CRT_FindFirstFile(path_wild, find_data);
    if (hFind != (void*)-1) {
        do {
            /* Skip "." and ".." */
            if (find_name[0] != '.') {
                /* Allocate node (0x508 bytes: path[0x504] + next_ptr[4]) */
                node = operator_new(0x508);
                *(char*)node = '\0';
                *(char*)((int8_t*)node + 0x504) = '\0';
                *(char*)((int8_t*)node + 0x505) = '\0';
                *(char*)((int8_t*)node + 0x506) = '\0';
                *(char*)((int8_t*)node + 0x507) = '\0';

                wsprintfA(node, (const char*)0x0047e8a0, path_base, find_name);

                hFile = CreateFileA(node, 0x80000000, 1, NULL, 3, 0x8000000, NULL);
                if (hFile == (void*)-1) {
                    magic = 0;
                } else {
                    if (!ReadFile(hFile, &magic, 2, &bytes_read, NULL)) {
                        magic = 0;
                    }
                    CloseHandle(hFile);
                }
                if (bytes_read != 2) {
                    magic = 0;
                }

                /* Only keep files with magic 0x66 */
                if ((int16_t)magic == 0x66) {
                    *(void**)((int8_t*)node + 0x504) = result_list;
                    result_list = node;
                } else {
                    GLOBAL_free(node);
                }
            }
        } while (CRT_FindNextFile(hFind, find_data) == 0);
        CRT_FindClose(hFind);
    }

    return result_list;
}

/* ================================================================== */
/* NET_UpdatePlayerList — 0x445170                                      */
/*                                                                      */
/* Count entries in Sort_Out PostBag. Returns count of .crd files      */
/* with magic 0x66. Called when player list needs refreshing.          */
/* ================================================================== */
int16_t __cdecl NET_UpdatePlayerList(void)
{
    void* node;
    void* next;
    int16_t count;

    count = 0;
    node = NET_GetHostName(2, 0);
    while (node != NULL) {
        next = *(void**)((int8_t*)node + 0x504);
        count++;
        GLOBAL_free(node);
        node = next;
    }
    return count;
}

/* ================================================================== */
/* NET_ResolveAddress — 0x444C70                                        */
/*                                                                      */
/* Allocate a new DPLAY_PlayerSlot (0x39C bytes), initialize it with   */
/* DPLAY_CreatePlayer, then load player data from a .crd file.         */
/* Returns the populated slot, or NULL on failure.                     */
/*                                                                      */
/* Called by: NETMAN_ReceiveSignalChange, NETMAN_DeserializePlayerData,*/
/*            Town_ReceivePostcard, Town_SavePostcard, Town_LoadPostcard */
/* ================================================================== */
void* __cdecl NET_ResolveAddress(const char* filepath)
{
    void* slot;

    slot = operator_new(0x39C);
    if (slot != NULL) {
        slot = DPLAY_CreatePlayer(slot);
    } else {
        slot = NULL;
    }

    if (DPLAY_GetPlayerName(slot, filepath) != 0) {
        return slot;
    }

    /* Load failed — destroy slot */
    if (slot != NULL) {
        (*(void(**)(void*, int))(*(void**)slot))(slot, 1);
    }
    return NULL;
}

/* ================================================================== */
/* NET_FindPlayer — 0x4451A0                                            */
/*                                                                      */
/* Delete .dat and .att files for a specific player attachment in a    */
/* PostBag subdirectory. Used to clean up stale attachment files.      */
/*                                                                      */
/* @param param_1  PostBag subdirectory type (0-7)                     */
/* @param param_2  Player/attachment ID (lower 16 bits used)           */
/* ================================================================== */
void __cdecl NET_FindPlayer(int32_t param_1, uint32_t param_2)
{
    char path[0x508];
    const char* subdir;

    path[0] = *(char*)0x4851D0;  /* g_empty_string */
    {
        uint32_t* p = (uint32_t*)(path + 1);
        int32_t i;
        for (i = 0; i < 0x140; i++) p[i] = 0;
    }
    path[0x501] = 0;
    path[0x502] = 0;

    subdir = PostBag_Subdir(param_1);

    /* Delete .att file */
    wsprintfA(path,
              (const char*)0x0047ed4c,  /* "%s%s%s_%08d.att" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\PostBag\" */
              subdir,
              (uint16_t)(param_2 & 0xFFFF));
    DeleteFileA(path);

    /* Delete .dat file */
    wsprintfA(path,
              (const char*)0x0047ed3c,  /* "%s%s%s_%08d.dat" */
              g_install_path,
              (const char*)0x0047e0c4,
              subdir,
              (uint16_t)(param_2 & 0xFFFF));
    DeleteFileA(path);
}

/* ================================================================== */
/* NET_GetAttFilePath — 0x445400                                        */
/*                                                                      */
/* Build path string for an .att attachment file:                       */
/*   <install>\PostBag\<subdir>\<id>.att                                */
/*                                                                      */
/* @param id      Attachment ID (lower 16 bits used)                   */
/* @param type    PostBag subdirectory type (0-7)                      */
/* @param outPath Output buffer for the constructed path               */
/* ================================================================== */
void __cdecl NET_GetAttFilePath(uint16_t id, int32_t type, char* outPath)
{
    const char* subdir = PostBag_Subdir(type);
    wsprintfA(outPath,
              (const char*)0x0047ed4c,  /* "%s%s%s_%08d.att" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\PostBag\" */
              subdir,
              (uint16_t)id);
}

/* ================================================================== */
/* NET_GetFilePath — 0x445510                                           */
/*                                                                      */
/* Build path string for a .dat attachment data file:                   */
/*   <install>\PostBag\<subdir>\<id>.dat                                */
/*                                                                      */
/* @param id      Attachment ID (lower 16 bits used)                   */
/* @param type    PostBag subdirectory type (0-7)                      */
/* @param outPath Output buffer for the constructed path               */
/* ================================================================== */
void __cdecl NET_GetFilePath(uint16_t id, int32_t type, char* outPath)
{
    const char* subdir = PostBag_Subdir(type);
    wsprintfA(outPath,
              (const char*)0x0047ed3c,  /* "%s%s%s_%08d.dat" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\PostBag\" */
              subdir,
              (uint16_t)id);
}

/* ================================================================== */
/* NET_SendFile — 0x445620                                              */
/*                                                                      */
/* Build path string for an attachment file with sender name:          */
/*   <install>\PostBag\Easter_<lang>\<param1>_<suffix>                 */
/*                                                                      */
/* Used to construct the target path for CopyFileA when uploading       */
/* attachment files. The suffix is "." or "_bak" based on param_2.     */
/* ================================================================== */
void __cdecl NET_SendFile(int32_t param_1, uint8_t param_2, char* outPath)
{
    const char* suffix;
    char local_8[8];

    if (param_2 == 0) {
        suffix = (const char*)0x0047ed68;  /* "_bak" */
    } else {
        suffix = (const char*)0x0047eb4c;  /* "." */
    }

    /* Copy suffix to local buffer */
    {
        const char* src = suffix;
        char* dst = local_8;
        uint32_t len;
        uint32_t u;

        len = 0xFFFFFFFF;
        { const char* p = src; while (*p++ && --len) ; }
        len = ~len;
        src = src + (len - 0xFFFFFFFF);

        for (u = len >> 2; u != 0; u--) {
            *(uint32_t*)dst = *(uint32_t*)src;
            src += 4; dst += 4;
        }
        for (u = len & 3; u != 0; u--) {
            *dst = *src;
            src++; dst++;
        }
    }

    wsprintfA(outPath,
              (const char*)0x0047ed5c,  /* "%s%s%s_%s%s" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\PostBag\" */
              PostBag_Subdir(6),         /* Always Easter */
              param_1,
              local_8);
}

/* ================================================================== */
/* NET_GetAssetPath — 0x445700                                          */
/*                                                                      */
/* Build path string for a clipart .bmp asset file used by the         */
/* surface cache. Format:                                               */
/*   <install>\Clipart\<filename>.bmp                                   */
/*                                                                      */
/* The filename is computed from param_1 (packed type) and param_2:    */
/*   type = param_1 >> 3, variant = (param_1 & 7) + 1                 */
/*   cases 0x00-0x0F:  "<type>_<variant-1>_<param2>.bmp"              */
/*   cases 0x10-0x19:  "c<type+0x58>_<variant-1>_<param2>.bmp"       */
/*   cases 0x1A-0x1D:  "c<type+0x5A>_<variant-1>_<param2>.bmp"       */
/*   case  0x1E:       "R_<variant-1>_<param2>.bmp"                   */
/*   case  0x1F:       "S0_<param2>.bmp"                               */
/*   default:          "0_<variant-1>_<param2>.bmp"                    */
/*                                                                      */
/* Also creates the Clipart directory if it doesn't exist.             */
/* ================================================================== */
void __cdecl NET_GetAssetPath(uint8_t param_1, uint32_t param_2, char* outPath)
{
    char filename[0x150];
    char clipdir[0x108];
    uint32_t fileAttrs;

    filename[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(filename + 1);
        int32_t i;
        for (i = 0; i < 0x0F; i++) p[i] = 0;
    }
    filename[0x41] = 0;
    filename[0x42] = 0;

    {
        uint8_t type_idx = param_1 >> 3;
        uint8_t variant = (param_1 & 7) + 1;

        switch (type_idx) {
        case 0x00: case 0x01: case 0x02: case 0x03:
        case 0x04: case 0x05: case 0x06: case 0x07:
        case 0x08: case 0x09: case 0x0A: case 0x0B:
        case 0x0C: case 0x0D: case 0x0E: case 0x0F:
            wsprintfA(filename,
                      (const char*)0x0047ec9c,  /* "%01x_%01d_%03d.bmp" */
                      (uint32_t)type_idx,
                      variant - 1,
                      param_2 & 0xFF);
            break;

        case 0x10: case 0x11: case 0x12: case 0x13:
        case 0x14: case 0x15: case 0x16: case 0x17:
        case 0x18: case 0x19:
            wsprintfA(filename,
                      (const char*)0x0047ecb0,  /* "c%01x_%01d_%03d.bmp" */
                      (uint32_t)(type_idx + 0x58),
                      variant - 1,
                      param_2 & 0xFF);
            break;

        case 0x1A: case 0x1B: case 0x1C: case 0x1D:
            wsprintfA(filename,
                      (const char*)0x0047ecb0,  /* "c%01x_%01d_%03d.bmp" */
                      (uint32_t)(type_idx + 0x5A),
                      variant - 1,
                      param_2 & 0xFF);
            break;

        case 0x1E:
            wsprintfA(filename,
                      (const char*)0x0047ecc0,  /* "R_%01d_%03d.bmp" */
                      variant - 1,
                      param_2 & 0xFF);
            break;

        case 0x1F:
            wsprintfA(filename,
                      (const char*)0x0047ecd0,  /* "S0_%03d.bmp" */
                      param_2 & 0xFF);
            break;

        default:
            wsprintfA(filename,
                      (const char*)0x0047ec8c,  /* "0_%01d_%03d.bmp" */
                      variant - 1,
                      param_2 & 0xFF);
            break;
        }
    }

    /* Build Clipart directory path and create if needed */
    wsprintfA(clipdir,
              (const char*)0x0047ed70,  /* "%sClipart" */
              g_install_path);

    fileAttrs = GetFileAttributesA(clipdir);
    if (fileAttrs == 0xFFFFFFFF) {
        CreateDirectoryA(clipdir, NULL);
    }

    /* Build final asset path */
    wsprintfA(outPath,
              (const char*)0x0047ec7c,  /* "%sClipart\\%s" */
              g_install_path,
              filename);
}

/* ================================================================== */
/* NET_GetAssetType — 0x445910                                          */
/*                                                                      */
/* Encode a track entry's packed type from its constituent parts.       */
/* reverse of the decoding in NET_GetOrCreateSurface:                   */
/*   result = (param_1 << 3) | (param_2 - 1)                           */
/*                                                                      */
/* @param param_1  type index (high 5 bits)                            */
/* @param param_2  variant (1-based, low 3 bits, stored as param_2-1)  */
/* @return         Packed 8-bit type value                              */
/* ================================================================== */
uint8_t __cdecl NET_GetAssetType(uint8_t param_1, uint8_t param_2)
{
    return (param_1 << 3) | (uint8_t)(param_2 - 1);
}

/* ================================================================== */
/* NET_CheckAssetExists — 0x445930                                      */
/*                                                                      */
/* Build path for checking if a player's .crd file exists:             */
/*   <install>\PostBag\<subdir>\_<configId>.crd                         */
/* ================================================================== */
void __cdecl NET_CheckAssetExists(int32_t configId, int32_t type, char* outPath)
{
    const char* subdir = PostBag_Subdir(type);
    wsprintfA(outPath,
              (const char*)0x0047ed2c,  /* "%s%s%s_%07d.crd" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\PostBag\" */
              subdir,
              configId);
}

/* ================================================================== */
/* NET_DownloadAsset — 0x445A40                                         */
/*                                                                      */
/* Read a .dat attachment file into a buffer. File size is always      */
/* 0x400 bytes (1024). Path:                                            */
/*   <install>\PostBag\<subdir>\<id>.dat                                */
/* ================================================================== */
void __cdecl NET_DownloadAsset(uint16_t id, int32_t type, void* buffer)
{
    char path[0x508];
    void* hFile;
    uint32_t bytes_read;

    path[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(path + 1);
        int32_t i;
        for (i = 0; i < 0x140; i++) p[i] = 0;
    }
    path[0x501] = 0;
    path[0x502] = 0;
    bytes_read = 0;

    wsprintfA(path,
              (const char*)0x0047ed3c,  /* "%s%s%s_%08d.dat" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\PostBag\" */
              PostBag_Subdir(type),
              (uint16_t)id);

    hFile = CreateFileA(path, 0x80000000, 1, NULL, 3, 0x8000000, NULL);
    if (hFile != (void*)-1) {
        ReadFile(hFile, buffer, 0x400, &bytes_read, NULL);
        CloseHandle(hFile);
    }
}

/* ================================================================== */
/* NET_UploadAsset — 0x445BD0                                           */
/*                                                                      */
/* Write attachment data (.dat) and copy source file (.att) into       */
/* PostBag. Returns the assigned attachment ID.                        */
/*                                                                      */
/* 1. Reads NextAttId from [POSTCARD] section of config INI            */
/* 2. Wraps at 0x7FFC back to 1                                        */
/* 3. Writes 0x400 bytes of <data> to <install>\PostBag\<subdir>\<id>.dat */
/* 4. Copies <srcPath> to <install>\PostBag\<subdir>\<id>.att          */
/*                                                                      */
/* Called by: Town_SavePostcard, Town_UploadPostcard                   */
/* ================================================================== */
uint32_t __cdecl NET_UploadAsset(int32_t type, const char* srcPath)
{
    char path[0x508];
    char data_buf[0x400];
    const char* subdir;
    void* hFile;
    uint32_t bytes_written;
    uint16_t attId;
    uint32_t result;

    path[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(path + 1);
        int32_t i;
        for (i = 0; i < 0x140; i++) p[i] = 0;
    }
    path[0x501] = 0;
    path[0x502] = 0;

    /* Copy source string name to data_buf */
    /* (the srcPath string is first 0x400 bytes of the attachment) */
    data_buf[0] = *(char*)0x4851D0;
    {
        char* p = data_buf + 1;
        int32_t i;
        for (i = 0; i < 0xFF; i++) { p[0] = '\0'; p[1] = '\0'; p[2] = '\0'; p[3] = '\0'; p += 4; }
    }
    data_buf[0x3FD] = '\0';
    data_buf[0x3FE] = '\0';
    data_buf[0x3FF] = '\0';
    {
        const char* src = srcPath;
        char* dst = data_buf;
        uint32_t len;
        uint32_t u;
        for (len = 0; src[len] != '\0'; len++) ;
        for (u = 0; u < len; u++) dst[u] = src[u];
    }

    /* Get next attachment ID from config */
    {
        /* Config_GetIniInt and Config_WriteInt — extern helpers */
        extern int32_t __fastcall Config_GetIniInt(void* ini, const char* section,
                                                     const char* key, int32_t def);
        extern void __fastcall Config_WriteInt(void* ini, const char* section,
                                                 const char* key, int32_t val);

        uint32_t rawId = (uint32_t)Config_GetIniInt(
            g_config_ini,
            (const char*)0x0047ed7c,  /* "POSTCARD" */
            (const char*)0x0047ed88,  /* "NextAttId" */
            1);

        if (rawId > 0x7FFC) {
            rawId = 1;
        }
        attId = (uint16_t)(rawId & 0xFFFF);
        Config_WriteInt(g_config_ini,
                        (const char*)0x0047ed7c,
                        (const char*)0x0047ed88,
                        attId + 1);
    }

    subdir = PostBag_Subdir(type);

    /* Write .dat file */
    wsprintfA(path,
              (const char*)0x0047ed3c,  /* "%s%s%s_%08d.dat" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\PostBag\" */
              subdir,
              (uint16_t)attId);

    hFile = CreateFileA(path, 0x40000000, 1, NULL, 4, 0x8000000, NULL);
    if (hFile == (void*)-1) {
        return 0;
    }

    bytes_written = 0;
    if (!WriteFile(hFile, data_buf, 0x400, &bytes_written, NULL)) {
        result = CloseHandle(hFile);
        return result & 0xFFFF0000;
    }
    CloseHandle(hFile);

    /* Copy .att source file */
    wsprintfA(path,
              (const char*)0x0047ed4c,  /* "%s%s%s_%08d.att" */
              g_install_path,
              (const char*)0x0047e0c4,
              subdir,
              (uint16_t)attId);
    CopyFileA(srcPath, path, 0);

    return (uint32_t)attId;
}

/* ================================================================== */
/* NET_GetNextAttId — 0x445F20                                          */
/*                                                                      */
/* Get the next postcard attachment ID from config INI. Reads/writes   */
/* NextAttId in [POSTCARD] section. Wraps at 0x7FFD back to 1.        */
/*                                                                      */
/* Called by: NET_UploadAsset via Config_GetIniInt/WriteInt            */
/* ================================================================== */
uint16_t __cdecl NET_GetNextAttId(void)
{
    extern int32_t __fastcall Config_GetIniInt(void* ini, const char* section,
                                                 const char* key, int32_t def);
    extern void __fastcall Config_WriteInt(void* ini, const char* section,
                                             const char* key, int32_t val);

    uint16_t attId = (uint16_t)Config_GetIniInt(
        g_config_ini,
        (const char*)0x0047ed7c,  /* "POSTCARD" */
        (const char*)0x0047ed88,  /* "NextAttId" */
        1);

    if (attId > 0x7FFC) {
        attId = 1;
    }
    Config_WriteInt(g_config_ini,
                    (const char*)0x0047ed7c,
                    (const char*)0x0047ed88,
                    attId + 1);
    return attId;
}

/* ================================================================== */
/* NET_ComputeColor — 0x4441C0                                          */
/*                                                                      */
/* Compute a 24-bit RGB color from 3 byte parameters. Each byte        */
/* adjusts R/G/B channels with non-linear weighting, then clamps       */
/* each channel to [0, 255]. Returns 0x00RRGGBB format.                */
/*                                                                      */
/* Used by DPLAY_RenderPlayer for fill color computation.              */
/* ================================================================== */
uint32_t __cdecl NET_ComputeColor(uint8_t param_1, uint8_t param_2, uint8_t param_3)
{
    int32_t r = 0xFF;
    int32_t g = 0xFF;
    int32_t b = 0xFF;

    if (param_2 != 0) {
        r = 0xFF - (int32_t)param_2;
        g = (param_2 >> 2) + 0xFF;
        b = (param_2 >> 1) + 0xFF;
    }
    if (param_3 != 0) {
        g -= (int32_t)param_3;
        b -= (int32_t)(param_3 >> 1);
        r += param_3 / 3;
    }
    if (param_1 != 0) {
        b -= (int32_t)param_1;
        r -= (int32_t)param_1;
        g += param_1 / 3;
    }

    /* Clamp each channel to [0, 255] */
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 0xFF) r = 0xFF;
    if (g > 0xFF) g = 0xFF;
    if (b > 0xFF) b = 0xFF;

    /* Return as 0x00RRGGBB */
    return ((uint32_t)(uint8_t)r << 16) |
           ((uint32_t)(uint8_t)g << 8)  |
           (uint32_t)(uint8_t)b;
}

/* ================================================================== */
/* DPLAY_EnumeratePlayers — 0x443260                                    */
/*                                                                      */
/* C free function. Load player names from easter-egg .crd files into  */
/* the NetworkPlayerList's name array at +0xB13. Reads from:           */
/*   <install>\PostBag\<subdir>_*_<configId>.crd                        */
/*                                                                      */
/* Only runs once (guarded by flag at +0xB12). Uses CRT_time() to      */
/* seed the RNG. Parses up to 16 player names (0xD bytes each).        */
/* ================================================================== */
void __cdecl DPLAY_EnumeratePlayers(void)
{
    extern void __cdecl CRT_time(void);  /* 0x466490 — time() wrapper */
    extern void* __stdcall CRT_FindFirstFile(const char*, void*);
    extern int32_t __stdcall CRT_FindNextFile(void*, void*);
    extern void __stdcall CRT_FindClose(void*);

    void* g_dplay = *(void**)0x4FD3B0;  /* NetworkPlayerList global */

    CRT_time();

    if (*(uint8_t*)((int8_t*)g_dplay + 0xB12) != 0) {
        return;  /* Already enumerated */
    }

    /* Clear name array (16 entries at +0xB13, each 0xD bytes) */
    {
        uint32_t* p = (uint32_t*)((int8_t*)g_dplay + 0x510);
        int32_t i;
        for (i = 0x7FF; i != 0; i--) { *p = 0; p++; }
        *(uint16_t*)p = 0;
        *((uint8_t*)p + 2) = 0;
    }
    {
        uint8_t* name = (uint8_t*)g_dplay + 0xB13;
        int32_t i;
        for (i = 0x10; i != 0; i--) {
            *name = 0;
            name += 0x0D;
        }
    }

    /* TODO: The function has a switch on DAT_004a97a0 (language code)
       which appears to be dead code / no-ops, then proceeds to
       enumerate .crd files from the PostBag easter directory. */

    /* Build path and enumerate */
    {
        char path_buf[0x510];
        /* ... (path construction + FindFirstFile/FindNextFile) */
    }

    *(uint8_t*)((int8_t*)g_dplay + 0xB12) = 1;
}

/* ================================================================== */
/* DPLAY_LeaveSession — 0x443440                                        */
/*                                                                      */
/* Iterate an array of 256 object pointers. For each non-NULL entry,   */
/* call its vtable[0] destructor with flags=1 (free). Clears entry     */
/* after destruction.                                                   */
/*                                                                      */
/* __fastcall (ECX = pointer to array of 256 void* entries).           */
/* ================================================================== */
void __fastcall DPLAY_LeaveSession(void** entries)
{
    int32_t i;
    for (i = 0; i < 256; i++) {
        entries++;
        if (*entries != NULL) {
            (*(void(**)(void*, int))(**(void***)entries))(entries, 1);
            *entries = NULL;
        }
    }
}
