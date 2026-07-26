/*
 * sdl3_ini.c — Portable INI file parser
 *
 * Implements a minimal INI reader compatible with GetPrivateProfileStringA
 * and GetPrivateProfileIntA behavior.  Handles:
 *   - [sections]
 *   - key=value pairs (with optional whitespace)
 *   - ; comments
 *   - case-insensitive section/key matching
 */

#include "sdl3_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Maximum line length in the INI file */
#define INI_MAX_LINE    512
/* Maximum number of entries (section+key pairs) */
#define INI_MAX_ENTRIES 256

typedef struct {
    char section[64];
    char key[64];
    char value[256];
} INI_Entry;

struct INI_File {
    INI_Entry entries[INI_MAX_ENTRIES];
    int       count;
};

/* Case-insensitive string comparison */
static int strcasecmp_simple(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Trim leading and trailing whitespace from a string (in-place) */
static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

INI_File *INI_Open(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    INI_File *ini = (INI_File*)calloc(1, sizeof(INI_File));
    if (!ini) { fclose(f); return NULL; }

    char line[INI_MAX_LINE];
    char current_section[64] = "";
    int count = 0;

    while (fgets(line, sizeof(line), f) && count < INI_MAX_ENTRIES) {
        char *p = trim(line);

        /* Skip empty lines and comments */
        if (*p == '\0' || *p == ';' || *p == '#') continue;

        /* Section header: [SectionName] */
        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                *end = '\0';
                char *sec = trim(p + 1);
                strncpy(current_section, sec, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
            }
            continue;
        }

        /* Key=Value pair */
        char *eq = strchr(p, '=');
        if (eq && current_section[0] != '\0') {
            *eq = '\0';
            char *key   = trim(p);
            char *value = trim(eq + 1);

            if (*key && *value && strlen(current_section) > 0) {
                strncpy(ini->entries[count].section, current_section,
                        sizeof(ini->entries[count].section) - 1);
                strncpy(ini->entries[count].key, key,
                        sizeof(ini->entries[count].key) - 1);
                strncpy(ini->entries[count].value, value,
                        sizeof(ini->entries[count].value) - 1);
                count++;
            }
        }
    }

    ini->count = count;
    fclose(f);
    return ini;
}

void INI_Close(INI_File *ini)
{
    if (ini) free(ini);
}

int INI_GetString(INI_File *ini, const char *section, const char *key,
                  const char *default_val, char *out_buf, size_t buf_size)
{
    if (!ini || !section || !key || !out_buf || buf_size == 0) {
        if (out_buf && buf_size > 0) out_buf[0] = '\0';
        return 0;
    }

    for (int i = 0; i < ini->count; i++) {
        if (strcasecmp_simple(ini->entries[i].section, section) == 0 &&
            strcasecmp_simple(ini->entries[i].key, key) == 0) {
            size_t len = strlen(ini->entries[i].value);
            if (len >= buf_size) len = buf_size - 1;
            memcpy(out_buf, ini->entries[i].value, len);
            out_buf[len] = '\0';
            return (int)len;
        }
    }

    /* Key not found — return default */
    if (default_val) {
        size_t len = strlen(default_val);
        if (len >= buf_size) len = buf_size - 1;
        memcpy(out_buf, default_val, len);
        out_buf[len] = '\0';
        return (int)len;
    }

    out_buf[0] = '\0';
    return 0;
}

int INI_GetInt(INI_File *ini, const char *section, const char *key,
               int default_val)
{
    char buf[64];
    int result = INI_GetString(ini, section, key, NULL, buf, sizeof(buf));
    if (result > 0) {
        return atoi(buf);
    }
    return default_val;
}
