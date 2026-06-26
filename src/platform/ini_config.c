/*
 * Lego Loco (1998) - Native Linux Port
 * src/platform/ini_config.c — POSIX/inih replacement for Win32 INI file functions
 *
 * Replaces the following Win32 subsystems documented in src/audio/audio.c
 * (CIniFile class at FUN_00452d50..FUN_00452df0) and resources.c:
 *   KERNEL32.DLL GetPrivateProfileStringA  — read string from INI section/key
 *   KERNEL32.DLL GetPrivateProfileIntA     — read integer from INI section/key
 *   KERNEL32.DLL WritePrivateProfileStringA — write string to INI section/key
 *
 * WIN32 → LINUX API mapping table:
 *
 *   GetPrivateProfileStringA(section, key, defaultVal, buf, bufSize, path)
 *     -> ini_parse(path, handler_cb, &ctx)
 *        where handler_cb populates a flat key-value store when
 *        strcmp(section, ctx.section) && strcmp(name, ctx.key) match.
 *        On match: strncpy(buf, value, bufSize); else: strncpy(buf, defaultVal, bufSize)
 *
 *   GetPrivateProfileIntA(section, key, defaultVal, path)
 *     -> Loco_Ini_GetInt(path, section, key, defaultVal)
 *        Calls Loco_Ini_GetString then atoi; returns defaultVal if key absent.
 *
 *   WritePrivateProfileStringA(section, key, value, path)
 *     -> Loco_Ini_SetString(path, section, key, value)
 *        Reads the entire file, modifies the key in-memory, writes it back.
 *        Creates the file and [section] if they do not exist.
 *
 * INI files used by Lego Loco:
 *   art-res/LOCO.INI     — main game configuration
 *   BALANCING.INI        — per-object FPS limits
 *
 * Keys read at startup (from CIniFile_ReadInt / CIniFile_ReadString):
 *   [BALANCING]
 *     MinVehicleFPS  = 20   (CGWND::minVehicleFPS  at +0x11)
 *     MinBuildingFPS = 18   (CGWND::minBuildingFPS at +0x12)
 *     MinMinifigFPS  = 16   (CGWND::minMinifigFPS  at +0x13)
 *     MinFlyingFPS   = 14   (CGWND::minFlyingFPS   at +0x14)
 *
 *   [AUDIO]
 *     SoundEnabled  = 1
 *     MusicEnabled  = 1
 *     SoundVolume   = 75    (0–100)
 *     MusicVolume   = 75    (0–100)
 *
 *   [DISPLAY]
 *     FullScreen    = 1
 *     Width         = 640
 *     Height        = 480
 *
 *   [MOUSE]
 *     Setting1      = ""    (stored username; see CUserProfile_Construct)
 *
 *   [CLIENT]
 *     NextId        = 1     (next available user slot ID; see CIniFile_WriteInt)
 *
 *   [ScreenSaver]
 *     Sound         = 0     (0=play music during screensaver, 1=silent)
 *
 *   [Sound]
 *     VolumeLow     = 0x4B  (saved DirectSound volume levels; see DS_SaveAndShutdown)
 *     VolumeMed     = 0x4B
 *     VolumeHigh    = 0x4E
 *
 * inih library dependency:
 *   This file uses the inih single-file library (third_party/inih/ini.h).
 *   inih provides ini_parse(filename, handler, userdata) which calls the
 *   handler callback for every section/name/value triple in the file.
 *   Source: https://github.com/benhoyt/inih (MIT licence)
 *   Integration: #include "third_party/inih/ini.h" and compile ini.c together.
 *
 * WritePrivateProfileStringA replacement strategy:
 *   Win32 WritePrivateProfileStringA is atomic on Windows (uses file locking).
 *   The Linux replacement here reads the file, modifies the in-memory
 *   representation, then writes the whole file back.  This is safe for
 *   Lego Loco's single-process, single-threaded INI usage.
 *
 * Build dependencies: third_party/inih/ini.c (compile alongside this file)
 *   Compile with: -DINI_ALLOW_MULTILINE=0 -DINI_STOP_ON_FIRST_ERROR=0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>   /* stat(), mkdir() */

/*
 * inih header.  The path assumes the third_party directory at repo root.
 * WIN32: no inih; GetPrivateProfileStringA is a kernel32 built-in.
 * LINUX: inih provides the INI parsing infrastructure.
 */
#include "../../third_party/inih/ini.h"

#include "../core/loco_types.h"

/* =========================================================================
 * Internal types
 * ========================================================================= */

/*
 * IniKV — single key-value entry in a parsed section.
 * Used by the in-memory INI representation for write-back.
 */
typedef struct IniKV {
    char key[128];
    char value[512];
} IniKV;

/*
 * IniSection — one [section] with its key-value pairs.
 */
#define INI_MAX_KV_PER_SECTION 64
typedef struct IniSection {
    char   name[128];
    IniKV  pairs[INI_MAX_KV_PER_SECTION];
    int    count;
} IniSection;

/*
 * IniDoc — in-memory representation of an entire INI file.
 * Allocated on the stack or heap for read-modify-write operations.
 */
#define INI_MAX_SECTIONS 32
typedef struct IniDoc {
    IniSection sections[INI_MAX_SECTIONS];
    int        sectionCount;
} IniDoc;

/*
 * IniGetContext — passed to inih's handler callback during a single-key lookup.
 * Replaces the implicit GetPrivateProfileStringA parameter passing.
 */
typedef struct IniGetContext {
    const char *targetSection;
    const char *targetKey;
    char       *outBuf;
    int         outBufSize;
    int         found;          /* set to 1 when the key is matched */
} IniGetContext;

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/*
 * str_trim_inplace  —  strip leading/trailing whitespace in-place.
 * Used to normalise key and value strings read from the INI file.
 * Win32's GetPrivateProfileStringA silently trims whitespace;
 * inih preserves it, so we replicate the Win32 behaviour here.
 */
static void str_trim_inplace(char *s)
{
    char *end;
    char *start = s;

    /* Find first non-whitespace */
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    /* Trim trailing whitespace */
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
}

/*
 * str_icmp  —  case-insensitive string compare (replaces _stricmp on Win32).
 * Win32 GetPrivateProfileStringA section and key lookups are case-insensitive.
 * POSIX strcasecmp is not in C11 strict mode; provide our own.
 */
static int str_icmp(const char *a, const char *b)
{
    while (*a && *b) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0) return d;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/*
 * ini_get_handler  —  inih callback used by Loco_Ini_GetString.
 *
 * Called once for each section/key/value triple in the INI file.
 * Sets ctx->found and copies the value when the target section and key match.
 *
 * WIN32: GetPrivateProfileStringA does this internally in kernel32.
 * LINUX: inih provides the parsing loop; this function is the "body".
 *
 * Return 1 to continue parsing, 0 to stop early.
 */
static int ini_get_handler(void *user, const char *section,
                            const char *name, const char *value)
{
    IniGetContext *ctx = (IniGetContext *)user;

    /* WIN32: section matching is case-insensitive in kernel32 */
    /* LINUX: replicate with str_icmp */
    if (str_icmp(section, ctx->targetSection) == 0 &&
        str_icmp(name,    ctx->targetKey)     == 0)
    {
        /* Found the key — copy value and signal early exit */
        strncpy(ctx->outBuf, value, (size_t)(ctx->outBufSize - 1));
        ctx->outBuf[ctx->outBufSize - 1] = '\0';
        str_trim_inplace(ctx->outBuf);
        ctx->found = 1;

        /* Return 0 to stop parsing early (no equivalent in Win32;
         * kernel32 always reads the entire file but returns on first match) */
        return 0;
    }
    return 1; /* continue */
}

/* =========================================================================
 * Loco_Ini_GetString  —  replaces GetPrivateProfileStringA
 *
 * Reads a string value from [section] key in the INI file at 'path'.
 * If the section or key does not exist, copies 'defaultVal' into 'outBuf'.
 *
 * WIN32: GetPrivateProfileStringA(section, key, defaultVal, outBuf, bufSize, path)
 *   Opens the file, scans for [section], then scans for key= within that section.
 *   Writes the value to outBuf (truncated at bufSize-1) and null-terminates.
 *   Returns the number of characters copied (excluding null terminator).
 *
 * LINUX: ini_parse(path, ini_get_handler, &ctx)
 *   inih opens and parses the file, calling ini_get_handler for each entry.
 *   If the key is found (ctx.found==1), outBuf already contains the value.
 *   If not found (ctx.found==0), copy defaultVal into outBuf.
 *
 * Parameters:
 *   path       — path to the INI file (e.g. "art-res/LOCO.INI")
 *   section    — section name without brackets (e.g. "AUDIO")
 *   key        — key name (e.g. "SoundEnabled")
 *   defaultVal — fallback string if key absent (may be NULL → treated as "")
 *   outBuf     — caller-supplied output buffer
 *   bufSize    — size of outBuf in bytes
 *
 * Returns number of characters written to outBuf (excluding null terminator).
 * ========================================================================= */
int Loco_Ini_GetString(const char *path,
                       const char *section,
                       const char *key,
                       const char *defaultVal,
                       char       *outBuf,
                       int         bufSize)
{
    IniGetContext ctx;
    int           parseResult;

    if (outBuf == NULL || bufSize <= 0) return 0;

    /* Initialise context */
    ctx.targetSection = section;
    ctx.targetKey     = key;
    ctx.outBuf        = outBuf;
    ctx.outBufSize    = bufSize;
    ctx.found         = 0;

    /* Pre-fill with default in case the key is not found */
    if (defaultVal != NULL)
        strncpy(outBuf, defaultVal, (size_t)(bufSize - 1));
    else
        outBuf[0] = '\0';
    outBuf[bufSize - 1] = '\0';

    /* WIN32: GetPrivateProfileStringA(section, key, defaultVal, outBuf, bufSize, path)
     *   Kernel32 opens the file and scans internally.
     * LINUX: ini_parse(path, handler, &ctx)
     *   inih opens the file, calls ini_get_handler for every section/key/value. */
    parseResult = ini_parse(path, ini_get_handler, &ctx);

    if (parseResult < 0 && !ctx.found) {
        /* File not found or unreadable — already pre-filled with defaultVal */
        /* WIN32: GetPrivateProfileStringA silently returns defaultVal on missing file */
    }

    return (int)strlen(outBuf);
}

/* =========================================================================
 * Loco_Ini_GetInt  —  replaces GetPrivateProfileIntA
 *
 * Reads an integer from [section] key in the INI file.
 * Returns defaultVal if the section, key, or file does not exist.
 *
 * WIN32: GetPrivateProfileIntA(section, key, defaultVal, path)
 *   Same as GetPrivateProfileStringA but converts the string to int.
 *   Empty string or non-numeric value returns 0 (not defaultVal).
 *   Key absent returns defaultVal.
 *
 * LINUX: Loco_Ini_GetString + atoi
 *   If the key is not found, GetString returns the defaultVal string
 *   (which we format as a decimal integer), so atoi gives the right result.
 *
 * Parameters match CIniFile_ReadInt (FUN_00452d60) in audio.c.
 * ========================================================================= */
int Loco_Ini_GetInt(const char *path,
                    const char *section,
                    const char *key,
                    int         defaultVal)
{
    char  buf[64];
    char  defaultStr[32];
    int   len;

    /* Format defaultVal as a string for the GetString fallback */
    snprintf(defaultStr, sizeof(defaultStr), "%d", defaultVal);

    /* WIN32: GetPrivateProfileIntA(section, key, defaultVal, path) */
    /* LINUX: GetString then atoi */
    len = Loco_Ini_GetString(path, section, key, defaultStr, buf, sizeof(buf));
    if (len == 0) return defaultVal;

    /* WIN32: GetPrivateProfileIntA returns defaultVal if key is absent;
     *        atoi on empty or non-numeric string returns 0.
     *        We preserve the same behaviour: if GetString returned the
     *        default string, atoi on it gives defaultVal. */
    return atoi(buf);
}

/* =========================================================================
 * ini_doc_parse_handler  —  inih callback used by Loco_Ini_SetString
 *
 * Builds a full in-memory IniDoc from the parsed file.
 * Used to support WritePrivateProfileStringA's read-modify-write pattern.
 *
 * WIN32: WritePrivateProfileStringA reads the full file internally,
 *        modifies the relevant key, and writes back atomically.
 * LINUX: We replicate this by loading the entire file into IniDoc,
 *        mutating the key, then writing back with Ini_WriteDoc.
 * ========================================================================= */
static int ini_doc_parse_handler(void *user, const char *section,
                                 const char *name, const char *value)
{
    IniDoc *doc = (IniDoc *)user;
    int     si;
    IniSection *sec = NULL;

    /* Find or create section */
    for (si = 0; si < doc->sectionCount; si++) {
        if (str_icmp(doc->sections[si].name, section) == 0) {
            sec = &doc->sections[si];
            break;
        }
    }
    if (sec == NULL) {
        if (doc->sectionCount >= INI_MAX_SECTIONS) return 1; /* overflow */
        sec = &doc->sections[doc->sectionCount++];
        strncpy(sec->name, section, sizeof(sec->name) - 1);
        sec->name[sizeof(sec->name) - 1] = '\0';
        sec->count = 0;
    }

    /* Add key-value pair */
    if (sec->count < INI_MAX_KV_PER_SECTION) {
        strncpy(sec->pairs[sec->count].key,   name,  sizeof(sec->pairs[0].key)   - 1);
        strncpy(sec->pairs[sec->count].value, value, sizeof(sec->pairs[0].value) - 1);
        sec->pairs[sec->count].key[sizeof(sec->pairs[0].key) - 1]   = '\0';
        sec->pairs[sec->count].value[sizeof(sec->pairs[0].value) - 1] = '\0';
        sec->count++;
    }

    return 1; /* continue */
}

/*
 * Ini_WriteDoc  —  writes an IniDoc back to disk.
 *
 * Produces a standard Windows-compatible INI format:
 *   [SectionName]
 *   key=value
 *   (blank line between sections)
 *
 * WIN32: WritePrivateProfileStringA formats and writes internally.
 * LINUX: we write the file explicitly here.
 */
static int Ini_WriteDoc(const char *path, const IniDoc *doc)
{
    FILE *fp;
    int   si, ki;

    /* WIN32: WritePrivateProfileStringA writes atomically (temp file + rename).
     * LINUX: fopen with "w" truncates and rewrites. For production code,
     *        write to a temp file and rename for atomicity. */
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "Ini_WriteDoc: cannot open '%s' for write: %s\n",
                path, strerror(errno));
        return 0;
    }

    for (si = 0; si < doc->sectionCount; si++) {
        if (si > 0) fprintf(fp, "\n");
        fprintf(fp, "[%s]\n", doc->sections[si].name);
        for (ki = 0; ki < doc->sections[si].count; ki++) {
            fprintf(fp, "%s=%s\n",
                    doc->sections[si].pairs[ki].key,
                    doc->sections[si].pairs[ki].value);
        }
    }

    fclose(fp);
    return 1;
}

/* =========================================================================
 * Loco_Ini_SetString  —  replaces WritePrivateProfileStringA
 *
 * Writes a string value to [section] key in the INI file at 'path'.
 * Creates the file and/or the section if they do not exist.
 * If 'value' is NULL, the key is removed (mirrors Win32 behaviour).
 *
 * WIN32: WritePrivateProfileStringA(section, key, value, path)
 *   Reads the entire INI file, modifies the specified key in the
 *   specified section, and writes the file back atomically.
 *   If value == NULL: deletes the key.
 *   If section == NULL: deletes the section.
 *   Returns TRUE on success, FALSE on failure.
 *
 * LINUX: Read → IniDoc → modify → WriteDoc
 *   1. ini_parse(path, ini_doc_parse_handler, &doc)  — load full doc
 *   2. Find or create [section] in doc
 *   3. Find or create key in that section; set/delete value
 *   4. Ini_WriteDoc(path, &doc)                      — write back
 *
 * Parameters match CIniFile_WriteString (FUN_00452df0) and
 * CIniFile_WriteInt (FUN_00452db0) in audio.c.
 * ========================================================================= */
int Loco_Ini_SetString(const char *path,
                       const char *section,
                       const char *key,
                       const char *value)
{
    IniDoc     doc;
    IniSection *sec;
    IniKV      *pair;
    int         si, ki;

    memset(&doc, 0, sizeof(doc));

    /* WIN32: WritePrivateProfileStringA reads the whole file first.
     *        Missing file is silently created.
     * LINUX: ini_parse returns -1 if file not found; proceed with empty doc. */
    ini_parse(path, ini_doc_parse_handler, &doc);

    /* Find or create the target section */
    sec = NULL;
    for (si = 0; si < doc.sectionCount; si++) {
        if (str_icmp(doc.sections[si].name, section) == 0) {
            sec = &doc.sections[si];
            break;
        }
    }
    if (sec == NULL) {
        /* WIN32: WritePrivateProfileStringA creates a new section if absent */
        /* LINUX: append a new section to the doc */
        if (doc.sectionCount >= INI_MAX_SECTIONS) {
            fprintf(stderr, "Loco_Ini_SetString: section limit reached\n");
            return 0;
        }
        sec = &doc.sections[doc.sectionCount++];
        strncpy(sec->name, section, sizeof(sec->name) - 1);
        sec->name[sizeof(sec->name) - 1] = '\0';
        sec->count = 0;
    }

    /* Find or create the key within the section */
    pair = NULL;
    for (ki = 0; ki < sec->count; ki++) {
        if (str_icmp(sec->pairs[ki].key, key) == 0) {
            pair = &sec->pairs[ki];
            break;
        }
    }

    if (value == NULL) {
        /* WIN32: WritePrivateProfileStringA with value==NULL deletes the key */
        /* LINUX: shift remaining pairs down by one */
        if (pair != NULL) {
            int idx = (int)(pair - sec->pairs);
            memmove(&sec->pairs[idx], &sec->pairs[idx + 1],
                    (size_t)(sec->count - idx - 1) * sizeof(IniKV));
            sec->count--;
        }
    } else {
        if (pair == NULL) {
            /* New key */
            if (sec->count >= INI_MAX_KV_PER_SECTION) {
                fprintf(stderr, "Loco_Ini_SetString: key limit reached in [%s]\n",
                        section);
                return 0;
            }
            pair = &sec->pairs[sec->count++];
            strncpy(pair->key, key, sizeof(pair->key) - 1);
            pair->key[sizeof(pair->key) - 1] = '\0';
        }
        /* Update value */
        strncpy(pair->value, value, sizeof(pair->value) - 1);
        pair->value[sizeof(pair->value) - 1] = '\0';
    }

    /* WIN32: WritePrivateProfileStringA atomically writes back.
     * LINUX: rewrite the file (see Ini_WriteDoc for atomicity note). */
    return Ini_WriteDoc(path, &doc);
}

/* =========================================================================
 * Loco_Ini_SetInt  —  replaces WritePrivateProfileStringA (integer variant)
 *
 * Convenience wrapper: converts 'value' to a decimal string then delegates
 * to Loco_Ini_SetString.
 *
 * WIN32: CIniFile_WriteInt (FUN_00452db0) uses FUN_00467ea0 (custom itoa)
 *        then WritePrivateProfileStringA.
 * LINUX: snprintf(buf, sizeof(buf), "%u", value) then Loco_Ini_SetString.
 *
 * Called by CUserProfile_Construct to persist CLIENT/NextId.
 * Called by DS_SaveAndShutdown to persist Sound/Volume* levels.
 * ========================================================================= */
int Loco_Ini_SetInt(const char *path,
                    const char *section,
                    const char *key,
                    int         value)
{
    char buf[32];
    /* WIN32: FUN_00467ea0(value, buf, 10) — custom itoa base 10 */
    /* LINUX: snprintf */
    snprintf(buf, sizeof(buf), "%d", value);
    /* WIN32: WritePrivateProfileStringA(section, key, buf, path) */
    /* LINUX: Loco_Ini_SetString */
    return Loco_Ini_SetString(path, section, key, buf);
}

/* =========================================================================
 * Loco_Ini_LoadBalancing  —  reads BALANCING.INI into CGWND FPS limits
 *
 * Wraps all four GetPrivateProfileIntA calls made during game startup
 * to populate the FPS-limit fields in the CGWND struct.
 *
 * Original calls (inferred from CGWND struct offsets in loco_types.h):
 *   game->minVehicleFPS  = GetPrivateProfileIntA("BALANCING","MinVehicleFPS", 20,path)
 *   game->minBuildingFPS = GetPrivateProfileIntA("BALANCING","MinBuildingFPS",18,path)
 *   game->minMinifigFPS  = GetPrivateProfileIntA("BALANCING","MinMinifigFPS", 16,path)
 *   game->minFlyingFPS   = GetPrivateProfileIntA("BALANCING","MinFlyingFPS",  14,path)
 *
 * WIN32: GetPrivateProfileIntA (4 calls, BALANCING.INI)
 * LINUX: Loco_Ini_GetInt (4 calls, same file)
 * ========================================================================= */
void Loco_Ini_LoadBalancing(CGWND *game, const char *iniPath)
{
    if (game == NULL || iniPath == NULL) return;

    /* WIN32: GetPrivateProfileIntA("BALANCING","MinVehicleFPS",  20, path) */
    /* LINUX: Loco_Ini_GetInt(iniPath, "BALANCING", "MinVehicleFPS",  20) */
    game->minVehicleFPS  = (uint8_t)Loco_Ini_GetInt(iniPath,
                               "BALANCING", "MinVehicleFPS",  20);

    /* WIN32: GetPrivateProfileIntA("BALANCING","MinBuildingFPS", 18, path) */
    /* LINUX: Loco_Ini_GetInt(iniPath, "BALANCING", "MinBuildingFPS", 18) */
    game->minBuildingFPS = (uint8_t)Loco_Ini_GetInt(iniPath,
                               "BALANCING", "MinBuildingFPS", 18);

    /* WIN32: GetPrivateProfileIntA("BALANCING","MinMinifigFPS",  16, path) */
    /* LINUX: Loco_Ini_GetInt(iniPath, "BALANCING", "MinMinifigFPS",  16) */
    game->minMinifigFPS  = (uint8_t)Loco_Ini_GetInt(iniPath,
                               "BALANCING", "MinMinifigFPS",  16);

    /* WIN32: GetPrivateProfileIntA("BALANCING","MinFlyingFPS",   14, path) */
    /* LINUX: Loco_Ini_GetInt(iniPath, "BALANCING", "MinFlyingFPS",   14) */
    game->minFlyingFPS   = (uint8_t)Loco_Ini_GetInt(iniPath,
                               "BALANCING", "MinFlyingFPS",   14);

    fprintf(stderr, "Loco_Ini_LoadBalancing: vehicle=%d building=%d minifig=%d flying=%d\n",
            game->minVehicleFPS, game->minBuildingFPS,
            game->minMinifigFPS, game->minFlyingFPS);
}

/* =========================================================================
 * Loco_Ini_LoadAudioConfig  —  reads audio settings from LOCO.INI
 *
 * Reads the [AUDIO] section from the main game INI file.
 * Called during audio subsystem initialisation before Mix_OpenAudio.
 *
 * WIN32 (CIniFile_ReadInt / CIniFile_ReadString calls in audio.c):
 *   GetPrivateProfileIntA("AUDIO", "SoundEnabled",  1,  path)
 *   GetPrivateProfileIntA("AUDIO", "MusicEnabled",  1,  path)
 *   GetPrivateProfileIntA("AUDIO", "SoundVolume",   75, path)
 *   GetPrivateProfileIntA("AUDIO", "MusicVolume",   75, path)
 *
 * LINUX: Loco_Ini_GetInt with the same section/key/defaults.
 * ========================================================================= */
void Loco_Ini_LoadAudioConfig(const char *iniPath,
                              int *outSoundEnabled,
                              int *outMusicEnabled,
                              int *outSoundVolume,
                              int *outMusicVolume)
{
    /* WIN32: GetPrivateProfileIntA("AUDIO","SoundEnabled", 1, path) */
    /* LINUX: Loco_Ini_GetInt */
    if (outSoundEnabled)
        *outSoundEnabled = Loco_Ini_GetInt(iniPath, "AUDIO", "SoundEnabled", 1);

    /* WIN32: GetPrivateProfileIntA("AUDIO","MusicEnabled", 1, path) */
    /* LINUX: Loco_Ini_GetInt */
    if (outMusicEnabled)
        *outMusicEnabled = Loco_Ini_GetInt(iniPath, "AUDIO", "MusicEnabled", 1);

    /* WIN32: GetPrivateProfileIntA("AUDIO","SoundVolume", 75, path) */
    /* LINUX: Loco_Ini_GetInt */
    if (outSoundVolume)
        *outSoundVolume  = Loco_Ini_GetInt(iniPath, "AUDIO", "SoundVolume", 75);

    /* WIN32: GetPrivateProfileIntA("AUDIO","MusicVolume", 75, path) */
    /* LINUX: Loco_Ini_GetInt */
    if (outMusicVolume)
        *outMusicVolume  = Loco_Ini_GetInt(iniPath, "AUDIO", "MusicVolume", 75);
}

/* =========================================================================
 * Loco_Ini_SaveAudioConfig  —  writes audio settings to LOCO.INI
 *
 * Mirrors DS_SaveAndShutdown (FUN_0045bb20) which persists the volume
 * levels before shutting down DirectSound.
 *
 * WIN32 (CIniFile_WriteInt calls):
 *   WritePrivateProfileStringA("AUDIO", "SoundEnabled", str, path)
 *   WritePrivateProfileStringA("AUDIO", "MusicEnabled", str, path)
 *   WritePrivateProfileStringA("Sound", "VolumeLow",    str, path)
 *   WritePrivateProfileStringA("Sound", "VolumeMed",    str, path)
 *   WritePrivateProfileStringA("Sound", "VolumeHigh",   str, path)
 *
 * LINUX: Loco_Ini_SetInt (each call rewrites the file; could be batched
 * with a direct IniDoc approach for performance, but correctness first).
 * ========================================================================= */
void Loco_Ini_SaveAudioConfig(const char *iniPath,
                              int soundEnabled, int musicEnabled,
                              int soundVolume,  int musicVolume)
{
    /* WIN32: WritePrivateProfileStringA("AUDIO","SoundEnabled", str, path) */
    /* LINUX: Loco_Ini_SetInt */
    Loco_Ini_SetInt(iniPath, "AUDIO", "SoundEnabled", soundEnabled);

    /* WIN32: WritePrivateProfileStringA("AUDIO","MusicEnabled", str, path) */
    Loco_Ini_SetInt(iniPath, "AUDIO", "MusicEnabled", musicEnabled);

    /* WIN32: WritePrivateProfileStringA("AUDIO","SoundVolume", str, path) */
    Loco_Ini_SetInt(iniPath, "AUDIO", "SoundVolume",  soundVolume);

    /* WIN32: WritePrivateProfileStringA("AUDIO","MusicVolume", str, path) */
    Loco_Ini_SetInt(iniPath, "AUDIO", "MusicVolume",  musicVolume);
}

/* =========================================================================
 * Loco_Ini_LoadDisplayConfig  —  reads [DISPLAY] section from LOCO.INI
 *
 * WIN32 (GetPrivateProfileIntA calls during APP_InitWindow):
 *   GetPrivateProfileIntA("DISPLAY","FullScreen", 1,   path)
 *   GetPrivateProfileIntA("DISPLAY","Width",      640, path)
 *   GetPrivateProfileIntA("DISPLAY","Height",     480, path)
 *
 * LINUX: Loco_Ini_GetInt with the same defaults.
 * ========================================================================= */
void Loco_Ini_LoadDisplayConfig(const char *iniPath,
                                int *outFullscreen,
                                int *outWidth,
                                int *outHeight)
{
    /* WIN32: GetPrivateProfileIntA("DISPLAY","FullScreen", 1,   path) */
    /* LINUX: Loco_Ini_GetInt */
    if (outFullscreen)
        *outFullscreen = Loco_Ini_GetInt(iniPath, "DISPLAY", "FullScreen", 1);

    /* WIN32: GetPrivateProfileIntA("DISPLAY","Width",      640, path) */
    /* LINUX: Loco_Ini_GetInt */
    if (outWidth)
        *outWidth      = Loco_Ini_GetInt(iniPath, "DISPLAY", "Width",  640);

    /* WIN32: GetPrivateProfileIntA("DISPLAY","Height",     480, path) */
    /* LINUX: Loco_Ini_GetInt */
    if (outHeight)
        *outHeight     = Loco_Ini_GetInt(iniPath, "DISPLAY", "Height", 480);
}

/* =========================================================================
 * Loco_Ini_GetUsername  —  reads [MOUSE] Setting1 (stored username)
 *
 * Called by CUserProfile_Construct (FUN_00452e10) in audio.c step 2.
 * The username is stored in [MOUSE] Setting1 by Win32 WritePrivateProfileStringA.
 *
 * WIN32: GetPrivateProfileStringA("MOUSE","Setting1","", outBuf,maxLen,path)
 * LINUX: Loco_Ini_GetString("MOUSE","Setting1","",outBuf,maxLen)
 * ========================================================================= */
int Loco_Ini_GetUsername(const char *iniPath, char *outBuf, int bufSize)
{
    /* WIN32: GetPrivateProfileStringA("MOUSE", "Setting1", "", outBuf, bufSize, path) */
    /* LINUX: Loco_Ini_GetString with section="MOUSE", key="Setting1", default="" */
    return Loco_Ini_GetString(iniPath, "MOUSE", "Setting1", "", outBuf, bufSize);
}

/* =========================================================================
 * Loco_Ini_SetUsername  —  writes [MOUSE] Setting1 (username)
 *
 * Called by CUserProfile_Construct / save routines.
 *
 * WIN32: WritePrivateProfileStringA("MOUSE","Setting1", username, path)
 * LINUX: Loco_Ini_SetString("MOUSE","Setting1", username)
 * ========================================================================= */
int Loco_Ini_SetUsername(const char *iniPath, const char *username)
{
    /* WIN32: WritePrivateProfileStringA("MOUSE", "Setting1", username, path) */
    /* LINUX: Loco_Ini_SetString */
    return Loco_Ini_SetString(iniPath, "MOUSE", "Setting1", username);
}

/* =========================================================================
 * Loco_Ini_GetNextClientId  —  reads [CLIENT] NextId
 *
 * Used in CUserProfile_Construct and CUserProfile_LoadFromFile to assign
 * sequential player slot IDs (1..999).
 *
 * WIN32: GetPrivateProfileIntA("CLIENT","NextId",0,path)
 * LINUX: Loco_Ini_GetInt("CLIENT","NextId",0)
 * ========================================================================= */
int Loco_Ini_GetNextClientId(const char *iniPath)
{
    /* WIN32: GetPrivateProfileIntA("CLIENT", "NextId", 0, path) */
    /* LINUX: Loco_Ini_GetInt */
    return Loco_Ini_GetInt(iniPath, "CLIENT", "NextId", 0);
}

/* =========================================================================
 * Loco_Ini_SetNextClientId  —  writes [CLIENT] NextId
 *
 * WIN32: CIniFile_WriteInt (FUN_00452db0):
 *   FUN_00467ea0(value, buf, 10) [custom itoa]
 *   WritePrivateProfileStringA("CLIENT","NextId",buf,path)
 * LINUX: Loco_Ini_SetInt("CLIENT","NextId",value)
 * ========================================================================= */
int Loco_Ini_SetNextClientId(const char *iniPath, int nextId)
{
    /* WIN32: WritePrivateProfileStringA("CLIENT", "NextId", itoa(nextId), path) */
    /* LINUX: Loco_Ini_SetInt */
    return Loco_Ini_SetInt(iniPath, "CLIENT", "NextId", nextId);
}

/* =========================================================================
 * Loco_Ini_GetScreensaverSound  —  reads [ScreenSaver] Sound
 *
 * Called by CScreenSaver_PlayMusic (FUN_004480c0) in audio.c.
 * Returns 0 = play music during screensaver, 1 = silent.
 *
 * WIN32: GetPrivateProfileIntA("ScreenSaver","Sound",0,path)
 * LINUX: Loco_Ini_GetInt("ScreenSaver","Sound",0)
 * ========================================================================= */
int Loco_Ini_GetScreensaverSound(const char *iniPath)
{
    /* WIN32: GetPrivateProfileIntA("ScreenSaver", "Sound", 0, path) */
    /* LINUX: Loco_Ini_GetInt */
    return Loco_Ini_GetInt(iniPath, "ScreenSaver", "Sound", 0);
}

/* =========================================================================
 * Loco_Ini_EnsureDefaults  —  creates LOCO.INI with defaults if absent
 *
 * On first run, Win32 WritePrivateProfileStringA would silently create the
 * file.  This function pre-creates it with all known keys so the game
 * always has a valid configuration to read.
 *
 * WIN32: No equivalent; Win32 GetPrivateProfileIntA returns defaultVal
 *        if the file is missing and never creates the file.
 *        WritePrivateProfileStringA creates the file on first write.
 * LINUX: Call this once at startup so inih never sees a missing file.
 * ========================================================================= */
void Loco_Ini_EnsureDefaults(const char *iniPath)
{
    FILE *fp;

    /* Check if file already exists */
    fp = fopen(iniPath, "r");
    if (fp != NULL) {
        fclose(fp);
        return; /* file exists — do not overwrite user settings */
    }

    /* Create with default values */
    fp = fopen(iniPath, "w");
    if (fp == NULL) {
        fprintf(stderr, "Loco_Ini_EnsureDefaults: cannot create '%s': %s\n",
                iniPath, strerror(errno));
        return;
    }

    fprintf(fp,
        "[BALANCING]\n"
        "MinVehicleFPS=20\n"
        "MinBuildingFPS=18\n"
        "MinMinifigFPS=16\n"
        "MinFlyingFPS=14\n"
        "\n"
        "[AUDIO]\n"
        "SoundEnabled=1\n"
        "MusicEnabled=1\n"
        "SoundVolume=75\n"
        "MusicVolume=75\n"
        "\n"
        "[DISPLAY]\n"
        "FullScreen=1\n"
        "Width=640\n"
        "Height=480\n"
        "\n"
        "[MOUSE]\n"
        "Setting1=\n"
        "\n"
        "[CLIENT]\n"
        "NextId=1\n"
        "\n"
        "[ScreenSaver]\n"
        "Sound=0\n"
        "\n"
        "[Sound]\n"
        "VolumeLow=75\n"
        "VolumeMed=75\n"
        "VolumeHigh=78\n"
    );

    fclose(fp);
    fprintf(stderr, "Loco_Ini_EnsureDefaults: created '%s' with defaults\n", iniPath);
}
