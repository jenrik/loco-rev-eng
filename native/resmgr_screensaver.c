/**
 * resmgr_screensaver.c — Screensaver enumeration and selection
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These two functions manage screensaver .sav file selection:
 *   - RESMGR_EnumScreenSavers (static/internal — no other caller in the
 *     tree): enumerates *.sav files in SaveGame/ or ScrSaver/
 *   - RESMGR_SelectScreensaver: picks a random screensaver, writes its path
 *
 * Calling convention: Ghidra disassembly of both functions (0x4481B0,
 * 0x448390) shows `RET 4` — one callee-cleaned stack dword — not the
 * `__fastcall`/`__cdecl` this file and resources/ResourceManager.h
 * previously (independently) claimed. Both also read/forward ECX at
 * entry (RESMGR_SelectScreensaver: `MOV ESI,ECX` then, later, `MOV
 * ECX,ESI` immediately before calling RESMGR_EnumScreenSavers) but the
 * callee never reads it — RESMGR_EnumScreenSavers' own explicit parameter
 * is stack-passed (`MOV AL, [ESP+0xB38]`). This is __stdcall with a
 * vestigial, functionally-dead ECX passthrough; corrected here and in
 * ResourceManager.h.
 */

#include "../shared/types.h"
#include "../resources/ResourceManager.h"
#include <cstring>

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

extern int   __stdcall wsprintfA(char* buf, const char* fmt, ...);            /* 0x477370 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern void* g_config_ini;          /* 0x4A9EEC — config/INI manager (Config struct) */
extern char  g_install_path[];      /* 0x4A99C8 — installation directory path */

/* String literals (from Ghidra data listings) */
static const char s_ScreenSaver[] = "ScreenSaver";       /* 0x47E2B4 */
static const char s_Random[] = "Random";                 /* 0x47EE58 */
static const char s_Layout[] = "Layout";                 /* 0x47EE3C */
static const char s_ScrSaver_out_fmt[] = "ScrSaver\\%s"; /* 0x47EE30 — output format "ScrSaver\\%s" */
static const char s_SaveGame_wild[] = "%s\\SaveGame\\*.sav";  /* 0x47EE60 */
static const char s_ScrSaver_wild[] = "%s\\ScrSaver\\*.sav";  /* 0x47EE74 */

/* WIN32_FIND_DATAA size: 0x140 bytes (320) for Win9x/ME. No canonical
 * struct exists in this tree (nothing but cFileName, at the standard
 * Win32 offset 0x2C, is ever read) — kept as a raw buffer per the
 * project's existing convention for opaque external-API structs. */
#define WIN32_FIND_DATA_SIZE 0x140
#define FIND_DATA_CFILENAME_OFFSET 0x2C

/* ================================================================== */
/* Screensaver enumeration node — one per .sav file found.             */
/*                                                                      */
/* Real layout confirmed via Ghidra disassembly of 0x448390: allocated  */
/* with `operator_new(0x508)`; the filename is copied starting at      */
/* offset 0, and exactly one 4-byte "next" pointer field lives at      */
/* offset 0x504 (`MOV dword ptr [node+0x504], ...`, read/written twice: */
/* zeroed defensively before the filename copy, then set to the prior  */
/* list head after it). A previous version of this file additionally  */
/* zeroed dwords at offsets 0x508 and 0x50C on every allocation — those */
/* offsets are one and two dwords *past* the 0x508-byte allocation      */
/* (out-of-bounds heap writes on every node; not present in the real    */
/* disassembly at all). Using a named struct instead of raw offset      */
/* arithmetic makes that class of bug structurally impossible. The      */
/* struct is host-native-sized (an 8-byte pointer here, vs. 4 in the    */
/* original x86 binary) per CLAUDE.md's host-layout-parity non-goal —   */
/* every access goes through operator_new(sizeof(ScreenSaverNode)),     */
/* never a hardcoded byte count. */
struct ScreenSaverNode {
    char filename[0x504];
    ScreenSaverNode* next;
};

/* ================================================================== */
/* RESMGR_EnumScreenSavers                                             */
/* Address: 0x448390                                                    */
/*                                                                      */
/* Enumerates .sav files from SaveGame/ or ScrSaver/ directory.         */
/* Builds a singly linked list, most-recently-found node first.         */
/*                                                                      */
/* scrSaverMode: 0 = enumerate SaveGame/*.sav, non-zero = ScrSaver/*.sav*/
/*                                                                      */
/* Returns: head pointer to linked list, or NULL if no files found.     */
/* The caller must walk the list and free nodes with GLOBAL_free.       */
/*                                                                      */
/* No caller outside this file (grepped tree-wide) — kept internal.     */
/* ================================================================== */
static ScreenSaverNode* __stdcall RESMGR_EnumScreenSavers(char scrSaverMode)
{
    char findPath[0x140];                  /* search path buffer (260+ bytes) */
    char findData[WIN32_FIND_DATA_SIZE];   /* WIN32_FIND_DATAA */
    ScreenSaverNode* head = nullptr;       /* linked list head (returned) */
    HANDLE hFind;

    std::memset(findPath, 0, sizeof(findPath));

    /* Build the search path: "%s\\SaveGame\\*.sav" or "%s\\ScrSaver\\*.sav" */
    {
        const char* fmt = (scrSaverMode == 0) ? s_SaveGame_wild : s_ScrSaver_wild;
        wsprintfA(findPath, fmt, g_install_path);
    }

    /* Find first .sav file */
    hFind = CRT_FindFirstFile(findPath, findData);
    if (hFind == reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1))) {
        return nullptr;
    }

    do {
        /* Skip entries starting with '.' (current/parent dir entries) */
        if (findData[0] != '.') {
            ScreenSaverNode* node =
                static_cast<ScreenSaverNode*>(operator_new(sizeof(ScreenSaverNode)));

            node->next = nullptr;

            /* Copy filename from findData (cFileName at the standard
             * WIN32_FIND_DATAA offset 0x2C). Equivalent to strcpy: the
             * original's dword-at-a-time copy loop (preserved in this
             * file's prior version) writes the same bytes as strcpy up
             * to and including the NUL, only differing in that it may
             * also write up to 3 further bytes of alignment padding
             * past the NUL — into freshly-allocated, not-yet-read
             * memory that strcpy simply leaves untouched. Both are
             * observationally identical to every reader (all of which
             * stop at the NUL), so strcpy is used here. */
            std::strcpy(node->filename, findData + FIND_DATA_CFILENAME_OFFSET);

            /* Prepend to linked list */
            node->next = head;
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
/* outBuf: output buffer (caller-allocated, >= 128 bytes)               */
/* ================================================================== */
void __stdcall RESMGR_SelectScreensaver(char* outBuf)
{
    char selected[0x80];   /* 128-byte local buffer for selected name */

    std::memset(selected, 0, sizeof(selected));

    /* Check if Random screensaver is enabled */
    int randomEnabled = Config_GetIniInt(g_config_ini, s_ScreenSaver, s_Random, 0);
    if (randomEnabled != 0) {
        /* Enumerate available screensavers */
        ScreenSaverNode* list = RESMGR_EnumScreenSavers(1);  /* 1 = ScrSaver directory */
        Config_GetIniString(g_config_ini, s_ScreenSaver, s_Layout,
                            "ScrSaver\\saver.sav", selected, 0x80);

        /* Count total items in list */
        int total = 0;
        for (ScreenSaverNode* node = list; node != nullptr; node = node->next) {
            total++;
        }

        /* Seed random with current time */
        DWORD tick = CRT_timeGetTime(nullptr);
        CRT_srand(tick);

        /* Pick a random index */
        int pickIdx;
        if (total > 0) {
            pickIdx = CRT_rand() % total;
        } else if (total == 0) {
            pickIdx = 0;
        } else {
            /* total < 0: overflow-handling branch reproduced exactly from
             * the original (see 0x448264-0x44827F). */
            int diff = 2 - total;
            if (diff != 0) {
                pickIdx = (CRT_rand() % diff) + total - 1;
            } else {
                pickIdx = -1; /* fallthrough below will use selected default */
            }
        }

        /* Walk the list to the selected index, copy the filename, and
         * free every node (matching the original: the whole list is
         * always freed here, not just the unselected entries). */
        int idx = 0;
        ScreenSaverNode* node = list;
        while (node != nullptr) {
            if (idx == pickIdx) {
                /* Same strcpy-equivalence note as RESMGR_EnumScreenSavers.
                 * Unbounded, matching the original exactly: neither this
                 * nor the original binary clamps the copy to selected's
                 * 0x80-byte size, so a .sav filename longer than 0x7F
                 * bytes would overflow selected in both — preserved, not
                 * fixed, per CLAUDE.md (no invented behavior change). */
                std::strcpy(selected, node->filename);
            }
            ScreenSaverNode* nextNode = node->next;
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

    /* Copy selected name to output buffer (unbounded, matching the
     * original — see the strcpy-equivalence note above). */
    std::strcpy(outBuf, selected);
}
