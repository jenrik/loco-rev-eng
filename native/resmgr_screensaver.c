/**
 * resmgr_screensaver.c — Screensaver enumeration and selection
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These two C free functions manage screensaver .sav file selection:
 *   - RESMGR_EnumScreenSavers: enumerates *.sav files in SaveGame/ or ScrSaver/
 *   - RESMGR_SelectScreensaver: picks a random screensaver, writes its path
 *
 * Both use __fastcall convention (ECX = first param). The tiny hint-of-C++ is
 * that ECX is used, but there is no this pointer, vtable, or class context.
 * These are C functions compiled with the C++ compiler's __fastcall default.
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);        /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);            /* 0x465CD0 */

extern DWORD __cdecl CRT_timeGetTime(int* timer);       /* 0x466AF0 */
extern void  __cdecl CRT_srand(unsigned int seed);      /* 0x466140 */
extern int   __cdecl CRT_rand(void);                    /* 0x466150 */

extern int   __thiscall Config_GetIniInt(void* ini, const char* section,
                                          const char* key, int def);    /* 0x452D60 */
extern void  __thiscall Config_GetIniString(void* ini, const char* section,
                                             const char* key, const char* def,
                                             char* buf, int maxLen);    /* 0x452D80 */

extern HANDLE __stdcall CRT_FindFirstFile(const char* path, void* findData); /* 0x467A20 */
extern int   __stdcall CRT_FindNextFile(HANDLE handle, void* findData);      /* 0x467B50 */
extern BOOL  __stdcall CRT_FindClose(HANDLE handle);                          /* 0x467C70 */

extern int   __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...);       /* 0x466D60 */
extern int   __stdcall wsprintfA(char* buf, const char* fmt, ...);            /* 0x477370 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern void* g_config_ini;          /* 0x4A9EEC — config/INI manager (Config struct) */
extern char  g_install_path[];      /* 0x4A99C8 — installation directory path */
extern char  g_empty_string;        /* 0x4851D0 — empty string ("") */

/* String literals (from Ghidra data listings) */
static const char s_ScreenSaver[] = "ScreenSaver";       /* 0x47E2B4 */
static const char s_Random[] = "Random";                 /* 0x47EE58 */
static const char s_Layout[] = "Layout";                 /* 0x47EE3C */
static const char s_ScrSaver_fmt[] = "ScrSaver\\%s";     /* 0x47EE44 — actually "%s\\ScrSaver\\*.sav" style */
static const char s_ScrSaver_out_fmt[] = "ScrSaver\\%s"; /* 0x47EE30 — output format "ScrSaver\\%s" */
static const char s_SaveGame_wild[] = "%s\\SaveGame\\*.sav";  /* 0x47EE60 */
static const char s_ScrSaver_wild[] = "%s\\ScrSaver\\*.sav";  /* 0x47EE74 */

/* WIN32_FIND_DATA structure size: 0x140 bytes (320) for Win9x/ME */
#define WIN32_FIND_DATA_SIZE 0x140

/* ================================================================== */
/* RESMGR_EnumScreenSaver — linked list node sizes                     */
/* ================================================================== */
#define ENUM_NODE_SIZE 0x508  /* 1288 bytes: filename[0x504] + next[0x504] */
#define ENUM_NAME_LEN  0x504  /* max filename length including null */
#define ENUM_NEXT_OFF  0x504  /* next pointer offset within node */

/* ================================================================== */
/* RESMGR_EnumScreenSavers                                             */
/* Address: 0x448390                                                    */
/*                                                                      */
/* Enumerates .sav files from SaveGame/ or ScrSaver/ directory.         */
/* Builds a singly linked list of ENUM_NODE_SIZE-byte nodes.            */
/*                                                                      */
/* param_1: 0 = enumerate SaveGame/*.sav, non-zero = ScrSaver/*.sav    */
/*                                                                      */
/* Returns: head pointer to linked list, or NULL if no files found.     */
/* The caller must walk the list and free nodes with GLOBAL_free.       */
/* ================================================================== */
char* __fastcall RESMGR_EnumScreenSavers(char param_1)
{
    char  findPath[0x140];       /* search path buffer (260+ bytes) */
    char  findData[WIN32_FIND_DATA_SIZE];  /* WIN32_FIND_DATAA (0x140 = 320 bytes) */
    char* head = NULL;           /* linked list head (returned) */
    HANDLE hFind;

    /* Initialize findPath to empty string */
    findPath[0] = g_empty_string;
    {
        int* p = (int*)(findPath + 1);
        for (int i = 0; i < 0x140; i++) {
            *p++ = 0;
        }
        *(short*)p = 0;
        *(char*)((intptr_t)p + 2) = 0;
    }

    /* Build the search path: "%s\\SaveGame\\*.sav" or "%s\\ScrSaver\\*.sav" */
    {
        const char* fmt = (param_1 == 0) ? s_SaveGame_wild : s_ScrSaver_wild;
        wsprintfA(findPath, fmt, g_install_path);
    }

    /* Find first .sav file */
    hFind = CRT_FindFirstFile(findPath, findData);
    if (hFind == (HANDLE)0xFFFFFFFF) {
        return NULL;
    }

    do {
        /* Skip entries starting with '.' (current/parent dir entries) */
        if (findData[0] != '.') {
            /* Allocate a new node */
            char* node = (char*)operator_new(ENUM_NODE_SIZE);

            /* Zero out filename area and next pointer */
            node[0] = '\0';
            *(int*)(node + ENUM_NEXT_OFF + 0) = 0;
            *(int*)(node + ENUM_NEXT_OFF + 4) = 0;
            *(int*)(node + ENUM_NEXT_OFF + 8) = 0;

            /* Copy filename from findData (cFileName at offset 0x2C in WIN32_FIND_DATAA) */
            {
                const char* src = findData + 0x2C;  /* cFileName offset */
                char* dst = node;
                /* String copy loop — strlen + memcpy equivalent */
                int len = 0;
                const char* p = src;
                while (*p++ != '\0') len++;
                p = src;
                for (int i = 0; i < (len + 4) / 4; i++) {
                    *(int*)dst = *(int*)p;
                    dst += 4;
                    p += 4;
                }
                for (int i = 0; i < (len & 3); i++) {
                    *dst++ = *p++;
                }
            }

            /* Prepend to linked list */
            *(char**)(node + ENUM_NEXT_OFF) = head;
            head = node;
        }
    } while (CRT_FindNextFile(hFind, findData) == 0);  /* 0 = success */

    CRT_FindClose(hFind);
    return head;
}

/* ================================================================== */
/* RESMGR_SelectScreensaver                                            */
/* Address: 0x4481B0                                                    */
/*                                                                      */
/* Reads the ScreenSaver INI settings. If Random != 0, enumerates       */
/* available screensavers, picks one randomly, and writes the path      */
/* to the output buffer. If Random == 0, reads the Layout INI value     */
/* directly.                                                            */
/*                                                                      */
/* param_1 (ECX): output buffer (char*, caller-allocated, >= 128 bytes) */
/* ================================================================== */
void __fastcall RESMGR_SelectScreensaver(char* outBuf)
{
    char selected[0x80];   /* 128-byte local buffer for selected name */

    /* Initialize selected to empty string */
    selected[0] = g_empty_string;
    {
        int* p = (int*)(selected + 1);
        for (int i = 0; i < 0x1F; i++) {
            *p++ = 0;
        }
        *(short*)p = 0;
        *(char*)((intptr_t)p + 2) = 0;
    }

    /* Check if Random screensaver is enabled */
    int randomEnabled = Config_GetIniInt(g_config_ini, s_ScreenSaver, s_Random, 0);
    if (randomEnabled != 0) {
        /* Enumerate available screensavers */
        char* list = RESMGR_EnumScreenSavers(1);  /* 1 = ScrSaver directory */
        Config_GetIniString(g_config_ini, s_ScreenSaver, s_Layout,
                            "ScrSaver\\saver.sav", selected, 0x80);

        /* Count total items in list */
        int total = 0;
        char* node = list;
        while (node != NULL) {
            total++;
            node = *(char**)(node + ENUM_NEXT_OFF);
        }

        /* Seed random with current time */
        DWORD tick = CRT_timeGetTime(NULL);
        CRT_srand(tick);

        /* Pick a random index */
        int pickIdx;
        if (total > 0) {
            pickIdx = CRT_rand() % total;
        } else {
            /* total <= 0: handle edge case when total == 0 or negative */
            if (total == 0) {
                pickIdx = 0;
            } else {
                /* total < 0: this branch handles possible overflow from decrement */
                int diff = 2 - total;
                if (diff != 0) {
                    pickIdx = (CRT_rand() % diff) + total - 1;
                } else {
                    pickIdx = -1; /* fallthrough below will use selected default */
                }
            }
        }

        /* Walk the list to the selected index and copy the filename */
        int idx = 0;
        node = list;
        while (node != NULL) {
            if (idx == pickIdx) {
                /* Copy filename from node to selected buffer */
                const char* src = node;
                char* dst = selected;
                {
                    int len = 0;
                    const char* p = src;
                    while (*p++ != '\0') len++;
                    p = src;
                    for (int i = 0; i < (len + 4) / 4; i++) {
                        *(int*)dst = *(int*)p;
                        dst += 4;
                        p += 4;
                    }
                    for (int i = 0; i < (len & 3); i++) {
                        *dst++ = *p++;
                    }
                }
            }
            char* nextNode = *(char**)(node + ENUM_NEXT_OFF);
            GLOBAL_free(node);
            idx++;
            node = nextNode;
        }

        /* Format the output path */
        wsprintfA(outBuf, s_ScrSaver_out_fmt, selected);
        return;
    }

    /* Random disabled: read Layout INI value directly */
    Config_GetIniString(g_config_ini, s_ScreenSaver, s_Layout,
                        "ScrSaver\\saver.sav", selected, 0x80);

    /* Copy selected name to output buffer */
    {
        const char* src = selected;
        char* dst = outBuf;
        int len = 0;
        const char* p = src;
        while (*p++ != '\0') len++;
        p = src;
        for (int i = 0; i < (len + 4) / 4; i++) {
            *(int*)dst = *(int*)p;
            dst += 4;
            p += 4;
        }
        for (int i = 0; i < (len & 3); i++) {
            *dst++ = *p++;
        }
    }
}
