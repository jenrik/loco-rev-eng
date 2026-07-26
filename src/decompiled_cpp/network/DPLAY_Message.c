/**
 * DPLAY_Message.c — PostBag file cleanup / message queue helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * DPLAY_SendMessages and DPLAY_ReceiveMessage implement the "message"
 * sending system, which is actually file-based: it deletes files in
 * PostBag directories to process/clean up messages.
 *
 * DPLAY_GetMessageCount is a caching wrapper around NET_GetHostName
 * that counts .crd files in the Sort_Out directory.
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

extern char    g_install_path[];       /* 0x4A99C8 */
extern void    GLOBAL_free(void*);     /* 0x465CD0 */

extern int32_t __stdcall wsprintfA(char*, const char*, ...);
extern int32_t __stdcall DeleteFileA(const char*);
extern uint32_t __stdcall SetFileAttributesA(const char*, uint32_t);
extern void* __stdcall CRT_FindFirstFile(const char*, void*);
extern int32_t __stdcall CRT_FindNextFile(void*, void*);
extern int32_t __stdcall CRT_FindClose(void*);
extern void* __cdecl NET_GetHostName(int32_t param_1, int32_t param_2);

/* ================================================================== */
/* DPLAY_SendMessages — 0x443470                                        */
/*                                                                      */
/* C free function. Clean up all four PostBag subdirectories by        */
/* deleting matching files. Calls DPLAY_ReceiveMessage on:             */
/*   Sort_In, Sort_Out, Att_Out, Att_In                                */
/*                                                                      */
/* Called from the NetworkPlayerList destructor.                       */
/* ================================================================== */
void __cdecl DPLAY_SendMessages(void)
{
    extern void __cdecl DPLAY_ReceiveMessage(const char* path);
    char path_buf[0x508];

    path_buf[0] = *(char*)0x4851D0;  /* g_empty_string */
    {
        uint32_t* p = (uint32_t*)(path_buf + 1);
        int32_t i;
        for (i = 0x140; i != 0; i--) { *p = 0; p++; }
    }
    path_buf[0x501] = 0;
    path_buf[0x502] = 0;

    /* Sort_In */
    wsprintfA(path_buf,
              (const char*)0x0047ec64,  /* "%s%s%s\\*" */
              g_install_path,
              (const char*)0x0047e0c4,  /* "\\PostBag\\" */
              (const char*)0x0047ebc4); /* "Sort_In" */
    DPLAY_ReceiveMessage(path_buf);

    /* Sort_Out */
    wsprintfA(path_buf,
              (const char*)0x0047ec64,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047ebb8); /* "Sort_Out" */
    DPLAY_ReceiveMessage(path_buf);

    /* Att_Out */
    wsprintfA(path_buf,
              (const char*)0x0047ec64,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047eb90); /* "Att_Out" */
    DPLAY_ReceiveMessage(path_buf);

    /* Att_In */
    wsprintfA(path_buf,
              (const char*)0x0047ec64,
              g_install_path,
              (const char*)0x0047e0c4,
              (const char*)0x0047eb9c); /* "Att_In" */
    DPLAY_ReceiveMessage(path_buf);
}

/* ================================================================== */
/* DPLAY_ReceiveMessage — 0x443550                                      */
/*                                                                      */
/* Delete all files matching a wildcard pattern. For each file found   */
/* (excluding "." and ".."), sets FILE_ATTRIBUTE_NORMAL and deletes.   */
/*                                                                      */
/* Called by: DPLAY_SendMessages (for all four PostBag subdirs)        */
/* ================================================================== */
void __cdecl DPLAY_ReceiveMessage(const char* pattern)
{
    char find_path[0x508];
    char full_path[0x508];
    void* hFind;
    void* find_data[5];  /* WIN32_FIND_DATAA */
    char find_name[260];
    int32_t i;

    /* Copy pattern to find_path */
    find_path[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(find_path + 1);
        for (i = 0x140; i != 0; i--) { *p = 0; p++; }
    }
    find_path[0x501] = 0;
    find_path[0x502] = 0;

    /* Copy pattern and null-terminate at first '*' if present */
    {
        const char* src = pattern;
        char* dst = find_path;
        uint32_t len;
        for (len = 0; src[len] != '\0'; len++) { dst[len] = src[len]; }
        dst[len] = '\0';
    }
    {
        /* Strip wildcard '* ' from the copy for directory base */
        uint32_t u;
        for (u = 0; find_path[u] != '\0'; u++) { if (find_path[u] == '*') break; }
        find_path[u] = '\0';
    }

    /* Initialize full_path */
    full_path[0] = *(char*)0x4851D0;
    {
        uint32_t* p = (uint32_t*)(full_path + 1);
        for (i = 0x140; i != 0; i--) { *p = 0; p++; }
    }
    full_path[0x501] = 0;
    full_path[0x502] = 0;

    hFind = CRT_FindFirstFile(pattern, find_data);
    if (hFind != (void*)-1) {
        do {
            if (find_name[0] != '.') {
                wsprintfA(full_path,
                          (const char*)0x0047e8a0,  /* "%s%s" */
                          find_path,
                          find_name);
                SetFileAttributesA(full_path, 0x80);  /* FILE_ATTRIBUTE_NORMAL */
                DeleteFileA(full_path);
            }
        } while (CRT_FindNextFile(hFind, find_data) == 0);
        CRT_FindClose(hFind);
    }
}

/* ================================================================== */
/* DPLAY_GetMessageCount — 0x443670                                     */
/*                                                                      */
/* Count entries in the Sort_Out PostBag directory. Uses a negative    */
/* cache sentinel at +0xB10: on first call, counts via NET_GetHostName, */
/* stores the count, and returns it. Subsequent calls return cached.   */
/*                                                                      */
/* __fastcall (ECX = NetworkPlayerList instance).                      */
/*                                                                      */
/* Called by: NETMAN_DeserializePlayerData, Town_PostcardUpdateUI,     */
/*            Town_PostcardDlgProc, Town_ReceivePostcard,               */
/*            Town_SavePostcard, Town_LoadPostcard, Town_DeletePostcard */
/* ================================================================== */
int16_t __fastcall DPLAY_GetMessageCount(void* networkPlayerList)
{
    int16_t count;

    count = *(int16_t*)((int8_t*)networkPlayerList + 0xB10);
    if (count >= 0) {
        return count;  /* Already cached */
    }

    /* First call — enumerate and cache */
    {
        void* node;
        void* next;

        count = 0;
        node = NET_GetHostName(2, 0);
        while (node != NULL) {
            next = *(void**)((int8_t*)node + 0x504);
            count++;
            GLOBAL_free(node);
            node = next;
        }
        *(int16_t*)((int8_t*)networkPlayerList + 0xB10) = count;
    }

    return count;
}
