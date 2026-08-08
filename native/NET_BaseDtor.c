/**
 * NET_GetNextAttId — Get next postcard attachment ID from config
 * Address: 0x445F20
 * Size: 73 bytes
 * Calling convention: __cdecl
 *
 * Ghidra's own auto-analysis names this function NET_GetNextAttId — this
 * file was originally named/authored under the stale default decompiler
 * label "NET_BaseDtor", which is NOT a destructor (renamed per evidence;
 * see network/Netman.h, which previously carried the same misnomer).
 *
 * Reads NextAttId from the POSTCARD section of the config INI file,
 * increments it (wrapping at 0x7FFD), writes it back, and returns the
 * next ID value. Used when creating new postcard attachments.
 *
 * Called by:
 *   Train_HandleConnectionSetup (0x43B444) — Ghidra-confirmed sole caller
 *   (the file's earlier "Postcard_TakePhoto" caller annotation was wrong;
 *   corrected here).
 */
#include "../shared/types.h"

extern void* g_config_ini;              /* 0x4AA46C */

extern int32_t __cdecl Config_GetIniInt(void* ini, const char* section, const char* key, int32_t def);
extern void __cdecl    Config_WriteInt(void* ini, const char* section, const char* key, int32_t val);

/* String constants */
#define STR_POSTCARD     "POSTCARD"        /* 0x47ED7C */
#define STR_NEXT_ATT_ID  "NextAttId"       /* 0x47ED88 */

/* Canonical declaration lives in network/Netman.h; this local prototype
 * (matching it exactly) satisfies -Wmissing-declarations without pulling
 * in Netman.h's much larger include graph for this one free function. */
extern uint16_t __cdecl NET_GetNextAttId(void);

/* ================================================================== */
/* NET_GetNextAttId                                                    */
/* ================================================================== */
uint16_t __cdecl NET_GetNextAttId(void)
{
    uint16_t nextId;

    nextId = static_cast<uint16_t>(Config_GetIniInt(g_config_ini, STR_POSTCARD, STR_NEXT_ATT_ID, 1));
    if (nextId > 0x7FFC) {
        nextId = 1;
    }

    Config_WriteInt(g_config_ini, STR_POSTCARD, STR_NEXT_ATT_ID, nextId + 1);
    return nextId;
}
