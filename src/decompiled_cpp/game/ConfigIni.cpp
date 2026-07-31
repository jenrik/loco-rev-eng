/**
 * ConfigIni.cpp - Configuration INI file read/write helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Thin wrappers around Win32 profile string APIs. On non-Win32 platforms,
 * these return defaults and write to a log.
 *
 * Functions:
 *   Config_GetIniInt    (0x452D60) - Read an integer from INI
 *   Config_GetIniString (0x452D80) - Read a string from INI
 *   Config_WriteInt     (0x452DB0) - Write an integer to INI
 *   Config_ReadInt      (0x452DF0) - Write default integer to INI
 */

#include "../shared/types.h"
#include <cstring>

/* Public declarations used by the recovered C ABI callers. */
extern "C" int Config_GetIniInt(void*, const char*, const char*, int);
extern "C" int Config_GetIniString(void*, const char*, const char*, const char*, char*, int);
extern "C" int Config_WriteInt(void*, const char*, const char*, int);
extern "C" int Config_ReadInt(void*, const char*, const char*, const char*);

/* ================================================================== */
/* Config object layout:                                                */
/*   +0x04: const char* - INI file path                                */
/* ================================================================== */

/**
 * Config_GetIniInt - Read integer from INI section:key
 * Address: 0x452D60
 *
 * On Win32: calls GetPrivateProfileIntA.
 * On non-Win32: returns default value.
 */
extern "C" int Config_GetIniInt(void* /*cfg*/, const char* /*section*/,
                                  const char* /*key*/, int def)
{
    return def;
}

/**
 * Config_GetIniString - Read string from INI section:key
 * Address: 0x452D80
 *
 * On Win32: calls GetPrivateProfileStringA.
 * On non-Win32: copies default string to buffer.
 */
extern "C" int Config_GetIniString(void* /*cfg*/, const char* /*section*/,
                                     const char* /*key*/, const char* def,
                                     char* buf, int bufSize)
{
    if (def && buf && bufSize > 0) {
        int len = 0;
        while (def[len] && len < bufSize - 1) {
            buf[len] = def[len];
            len++;
        }
        buf[len] = '\0';
        return len;
    }
    if (buf && bufSize > 0) buf[0] = '\0';
    return 0;
}

/**
 * Config_WriteInt - Write integer value to INI
 * Address: 0x452DB0
 *
 * On Win32: calls WritePrivateProfileStringA.
 * On non-Win32: no-op, returns TRUE.
 */
extern "C" int Config_WriteInt(void* /*cfg*/, const char* /*section*/,
                                 const char* /*key*/, int /*value*/)
{
    return 1; /* TRUE */
}

/**
 * Config_WriteDefaultString - Write a default string value to INI
 * Address: 0x452DF0
 *
 * Despite the Ghidra name "Config_ReadInt", this function takes a
 * string value (LPCSTR) and writes it as the default for the given
 * section:key. It wraps WritePrivateProfileStringA directly.
 * The INI file path is at cfg+0x04.
 *
 * @param cfg     Config object pointer (filepath at +0x04)
 * @param section INI section name
 * @param key     INI key name
 * @param value   String value to write as default
 * @return        1 on success
 */
extern "C" int Config_ReadInt(void* /*cfg*/, const char* /*section*/,
                                const char* /*key*/, const char* /*value*/)
{
    /* On Win32: WritePrivateProfileStringA(section, key, value, cfg+4)
     * On non-Win32: no-op, returns success */
    return 1;
}
