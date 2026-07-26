/**
 * config_ini.c — Configuration INI file read/write helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Thin wrappers around Win32 GetPrivateProfileIntA,
 * GetPrivateProfileStringA, and WritePrivateProfileStringA.
 * Each accepts a "_this" pointer that points to an object whose
 * +0x04 field contains the INI file path string.
 *
 * Functions:
 *   Config_GetIniInt   — Read an integer from INI (0x452D60, 28 bytes)
 *   Config_GetIniString — Read a string from INI (0x452D80, 38 bytes)
 *   Config_WriteInt    — Write an integer to INI (0x452DB0, 58 bytes)
 *   Config_ReadInt     — Write default integer to INI (0x452DF0, 28 bytes)
 *
 * Calling convention: All __thiscall with ECX = config object pointer
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern UINT __stdcall GetPrivateProfileIntA(LPCSTR lpAppName, LPCSTR lpKeyName,
                                             INT nDefault, LPCSTR lpFileName);    /* 0x477384 */
extern DWORD __stdcall GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName,
                                                 LPCSTR lpDefault, LPSTR lpReturnedString,
                                                 DWORD nSize, LPCSTR lpFileName);  /* 0x477380 */
extern BOOL  __stdcall WritePrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName,
                                                   LPCSTR lpString, LPCSTR lpFileName); /* 0x477388 */
extern char* __cdecl CRT_itoa(int value, char* str, int radix);   /* 0x467EA0 */

/* ================================================================== */
/* Config_GetIniInt — Read integer from INI section:key               */
/* Address: 0x452D60                                                   */
/*                                                                     */
/* Returns: value from INI, or default_val if key not found.          */
/* INI file path is at _this+4.                                        */
/* ================================================================== */
UINT __thiscall Config_GetIniInt(void* _this, LPCSTR section, LPCSTR key, INT default_val)
{
    /* INI path stored in object at offset +0x04 */
    return GetPrivateProfileIntA(section, key, default_val, *(LPCSTR*)((char*)_this + 4));
}

/* ================================================================== */
/* Config_GetIniString — Read string from INI section:key             */
/* Address: 0x452D80                                                   */
/*                                                                     */
/* Returns: string in buffer, or default_str if key not found.        */
/* INI file path is at _this+4.                                        */
/* ================================================================== */
DWORD __thiscall Config_GetIniString(void* _this, LPCSTR section, LPCSTR key,
                                       LPCSTR default_str, LPSTR buffer, DWORD bufSize)
{
    return GetPrivateProfileStringA(section, key, default_str, buffer, bufSize,
                                     *(LPCSTR*)((char*)_this + 4));
}

/* ================================================================== */
/* Config_WriteInt — Write integer to INI section:key                 */
/* Address: 0x452DB0                                                   */
/*                                                                     */
/* Converts value to string via CRT_itoa, then writes to INI.         */
/* INI file path is at _this+4.                                        */
/* ================================================================== */
void __thiscall Config_WriteInt(void* _this, LPCSTR section, LPCSTR key, uint value)
{
    char buf[100];  /* local_64 */
    CRT_itoa((int)value, buf, 10);
    WritePrivateProfileStringA(section, key, buf, *(LPCSTR*)((char*)_this + 4));
}

/* ================================================================== */
/* Config_ReadInt — Write default integer to INI section:key          */
/* Address: 0x452DF0                                                   */
/*                                                                     */
/* Writes param_3 as a default string value to the INI file.          */
/* INI file path is at _this+4.                                        */
/* ================================================================== */
void __thiscall Config_ReadInt(void* _this, LPCSTR section, LPCSTR key, LPCSTR default_str)
{
    WritePrivateProfileStringA(section, key, default_str, *(LPCSTR*)((char*)_this + 4));
}
