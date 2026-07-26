/**
 * DPLAY_ReceiveMessage — Delete all files matching pattern in a postbag directory
 * Address: 0x443550
 * Size: 286 bytes
 * Calling convention: __stdcall
 *
 * Enumerates all files in the given directory path via FindFirstFile/FindNextFile.
 * For each file excluding dot entries ("."), sets FILE_ATTRIBUTE_NORMAL and
 * deletes it. Used to clean up PostBag directories.
 *
 * Called by:
 *   DPLAY_SendMessages (0x443470) — for each of four PostBag subdirectories
 */
#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern char g_empty_string;             /* 0x4851D0 */

/* Win32 API (called via IAT thunks) */
extern void* __stdcall CRT_FindFirstFile(const char* lpFileName, void* lpFindFileData);
extern int32_t __stdcall CRT_FindNextFile(void* hFindFile, void* lpFindFileData);
extern int32_t __stdcall CRT_FindClose(void* hFindFile);
extern int32_t __stdcall SetFileAttributesA(const char* lpFileName, uint32_t dwFileAttributes);
extern int32_t __stdcall DeleteFileA(const char* lpFileName);
extern int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);

/* Format string: "%s\\%s" */
#define FMT_FILE_PATH "%s\\%s"          /* 0x47E8A0 */

/* ================================================================== */
/* DPLAY_ReceiveMessage                                                */
/* ================================================================== */
void __stdcall DPLAY_ReceiveMessage(const char* path)
{
    char base_path[0x504];
    char file_path[0x504];
    uint8_t find_data[0x320];
    void* hFind;

    /* Copy base path (omit trailing backslash) */
    {
        int32_t len = 0;
        base_path[0] = g_empty_string;
        {
            uint32_t* p = (uint32_t*)(&base_path[1]);
            int32_t i;
            for (i = 0; i < 0x140; i++) {
                p[i] = 0;
            }
        }
        base_path[0x501] = 0;
        base_path[0x502] = 0;

        /* Copy path string */
        {
            const char* src = path;
            char* dst = base_path;
            while (*src != '\0') {
                *dst++ = *src++;
            }
            *dst = '\0';
        }

        /* Append "*" wildcard pattern for FindFirstFile */
        {
            len = 0;
            const char* s = base_path;
            while (*s != '\0') { s++; len++; }
        }
        base_path[len] = '*';
        base_path[len + 1] = '\0';
    }

    /* Initialize file_path buffer */
    file_path[0] = g_empty_string;
    {
        uint32_t* p = (uint32_t*)(&file_path[1]);
        int32_t i;
        for (i = 0; i < 0x140; i++) {
            p[i] = 0;
        }
    }
    file_path[0x501] = 0;
    file_path[0x502] = 0;

    /* Enumerate files */
    hFind = CRT_FindFirstFile(base_path, find_data);
    if (hFind != (void*)-1) {
        do {
            char* filename = (char*)find_data + 0x2C;  /* cFileName in WIN32_FIND_DATA */
            if (filename[0] != '.') {
                wsprintfA(file_path, FMT_FILE_PATH, base_path, filename);
                SetFileAttributesA(file_path, 0x80);  /* FILE_ATTRIBUTE_NORMAL */
                DeleteFileA(file_path);
            }
        } while (CRT_FindNextFile(hFind, find_data) == 0);

        CRT_FindClose(hFind);
    }
}
