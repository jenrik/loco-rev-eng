/**
 * NET_BaseDtor — Get next postcard attachment ID from config
 * Address: 0x445F20
 * Size: 73 bytes
 * Calling convention: __cdecl
 *
 * NOT a destructor despite the name. Reads NextAttId from the POSTCARD section
 * of the config INI file, increments it (wrapping at 0x7FFD), writes it back,
 * and returns the next ID value. Used when creating new postcard attachments.
 *
 * Called by:
 *   Postcard_TakePhoto (for assigning attachment IDs)
 */
#include "../shared/types.h"

extern void* g_config_ini;              /* 0x4AA46C */

extern int32_t __cdecl Config_GetIniInt(void* ini, const char* section, const char* key, int32_t def);
extern void __cdecl    Config_WriteInt(void* ini, const char* section, const char* key, int32_t val);

/* String constants */
#define STR_POSTCARD     "POSTCARD"        /* 0x47ED7C */
#define STR_NEXT_ATT_ID  "NextAttId"       /* 0x47ED88 */

/* ================================================================== */
/* NET_BaseDtor                                                        */
/* ================================================================== */
uint16_t __cdecl NET_BaseDtor(void)
{
    uint16_t nextId;

    nextId = (uint16_t)Config_GetIniInt(g_config_ini, STR_POSTCARD, STR_NEXT_ATT_ID, 1);
    if (nextId > 0x7FFC) {
        nextId = 1;
    }

    Config_WriteInt(g_config_ini, STR_POSTCARD, STR_NEXT_ATT_ID, nextId + 1);
    return nextId;
}
