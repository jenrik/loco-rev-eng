/**
 * DPLAY_SendMessages — Process all four postbag directories
 * Address: 0x443470
 * Size: 218 bytes
 * Calling convention: __cdecl
 *
 * Iterates over four PostBag subdirectories (Sort_In, Sort_Out, Att_Out, Att_In)
 * under install_dir/PostBag/, calling DPLAY_ReceiveMessage on each to delete
 * matching files. Used as a cleanup routine during network player list teardown.
 *
 * Called by:
 *   DPLAY_TermPlayerList (0x4431F0) — destructor for NetworkPlayerList
 */
#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern char g_install_path[];           /* 0x4A99C8 */
extern char g_empty_string;             /* 0x4851D0 */
extern void __cdecl DPLAY_ReceiveMessage(const char* path);
extern int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);

/* Format string: "%s%s\\%s" */
#define FMT_POSTBAG_PATH "%s%s\\%s"     /* 0x47EC64 */

/* String constants */
#define STR_POSTBAG      "PostBag"      /* 0x47E0C4 */
#define STR_SORT_IN      "Sort_In"      /* 0x47EBC4 */
#define STR_SORT_OUT     "Sort_Out"     /* 0x47EBB8 */
#define STR_ATT_OUT      "Att_Out"      /* 0x47EB90 */
#define STR_ATT_IN       "Att_In"       /* 0x47EB9C */

/* ================================================================== */
/* DPLAY_SendMessages                                                  */
/* ================================================================== */
void __cdecl DPLAY_SendMessages(void)
{
    char buf[0x504];

    /* Initialize buffer */
    buf[0] = g_empty_string;
    {
        uint32_t* p = (uint32_t*)(&buf[1]);
        int32_t i;
        for (i = 0; i < 0x140; i++) {
            p[i] = 0;
        }
    }
    buf[0x501] = 0;
    buf[0x502] = 0;

    wsprintfA(buf, FMT_POSTBAG_PATH, g_install_path, STR_POSTBAG, STR_SORT_IN);
    DPLAY_ReceiveMessage(buf);

    wsprintfA(buf, FMT_POSTBAG_PATH, g_install_path, STR_POSTBAG, STR_SORT_OUT);
    DPLAY_ReceiveMessage(buf);

    wsprintfA(buf, FMT_POSTBAG_PATH, g_install_path, STR_POSTBAG, STR_ATT_OUT);
    DPLAY_ReceiveMessage(buf);

    wsprintfA(buf, FMT_POSTBAG_PATH, g_install_path, STR_POSTBAG, STR_ATT_IN);
    DPLAY_ReceiveMessage(buf);
}
