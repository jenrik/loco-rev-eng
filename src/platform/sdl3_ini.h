/*
 * sdl3_ini.h — Portable INI file parser
 *
 * Replaces: GetPrivateProfileStringA / GetPrivateProfileIntA (kernel32.dll)
 * Original addresses: 0x00477120 (GetPrivateProfileStringA import)
 *                      0x0047711C (GetPrivateProfileIntA import)
 *
 * Lego Loco reads lego.ini sections:
 *   [DIRECTORIES]  Install=, ResFile=, CDROM=, Data=
 *   [BALANCING]    MinVehicleFPS=, MinBuildingFPS=, MinMinifigFPS=
 *   [DISPLAY]      ScreenWidth=, ScreenHeight=, FullScreen=, BPP=
 *   [AUDIO]        Sound=, Music=, Volume=, Channels=
 */

#ifndef LOCO_SDL3_INI_H
#define LOCO_SDL3_INI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque INI file handle. */
typedef struct INI_File INI_File;

/**
 * INI_Open — Open and parse an INI file.
 *
 * @param path   File path to the .ini file.
 * @return       Handle, or NULL on failure.
 */
INI_File *INI_Open(const char *path);

/**
 * INI_Close — Close and free an INI file handle.
 */
void INI_Close(INI_File *ini);

/**
 * INI_GetString — Read a string value from a section/key.
 *
 * Equivalent to: GetPrivateProfileStringA(section, key, default, buf, size, path)
 *
 * @param ini      Opened INI file.
 * @param section  Section name (e.g., "BALANCING").
 * @param key      Key name (e.g., "MinVehicleFPS").
 * @param default_val  Default value if key not found.
 * @param out_buf  Output buffer.
 * @param buf_size Size of output buffer.
 * @return         Number of characters copied (excluding null).
 */
int INI_GetString(INI_File *ini, const char *section, const char *key,
                  const char *default_val, char *out_buf, size_t buf_size);

/**
 * INI_GetInt — Read an integer value.
 *
 * Equivalent to: GetPrivateProfileIntA(section, key, default, path)
 */
int INI_GetInt(INI_File *ini, const char *section, const char *key,
               int default_val);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_SDL3_INI_H */
