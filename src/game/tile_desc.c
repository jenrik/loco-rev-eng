/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * src/game/tile_desc.c — Tile descriptor (.dat) text parser
 *
 * The original game parses .dat files at startup from the RFH/RFD archive.
 * This is a clean re-implementation for the Linux port with identical semantics.
 *
 * .dat format: plain ASCII text, CRLF line endings, Windows backslash paths.
 * 627 .dat files exist in resource.RFD covering all tile types.
 *
 * WIN32: original parser in CGameWorld_LoadTileDescriptors (~0x42c000 area)
 *        used _stricmp for case-insensitive key matching, strtok for parsing.
 * LINUX: this implementation is equivalent; strcasecmp replaces _stricmp.
 *
 * Original: Intelligent Games for LEGO Media (1998)
 */

#include "tile_desc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #define STRCASECMP _stricmp
#else
  #include <strings.h>
  #define STRCASECMP strcasecmp
#endif

CTileDesc *g_tileDescs     = NULL;
int        g_tileDescCount = 0;

/* =========================================================================
 * Internal parser helpers
 * ========================================================================= */

/* Split one line into tokens; returns token count */
static int tokenize(char *line, char **tok, int max_tok) {
    int n = 0;
    char *p = line;
    while (*p && n < max_tok) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\r' || *p == '\n') break;
        tok[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

static int parse_int_default(const char *s, int def) {
    if (!s) return def;
    char *end;
    long v = strtol(s, &end, 10);
    return (end > s) ? (int)v : def;
}

/* Advance line pointer past CRLF/LF to next line */
static const char *next_line(const char *p) {
    while (*p && *p != '\n' && *p != '\r') p++;
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    return p;
}

/* Copy one line into buf (without \r\n); returns pointer to start of next line */
static const char *get_line(const char *p, char *buf, int buf_size) {
    int i = 0;
    while (*p && *p != '\n' && *p != '\r' && i < buf_size - 1)
        buf[i++] = *p++;
    buf[i] = '\0';
    return next_line(p);
}

/* Parse a row of integers from a line into arr; returns count read */
static int parse_int_row(const char *line, int *arr, int max_count) {
    int n = 0;
    const char *p = line;
    while (*p && n < max_count) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\r' || *p == '\n') break;
        char *end;
        long v = strtol(p, &end, 10);
        if (end > p) {
            arr[n++] = (int)v;
            p = end;
        } else {
            break;
        }
    }
    return n;
}

/* Skip blank lines; returns pointer to first non-blank line */
static const char *skip_blank(const char *p) {
    char buf[256];
    while (*p) {
        const char *next = get_line(p, buf, sizeof(buf));
        char *s = buf;
        while (*s == ' ' || *s == '\t') s++;
        if (*s && *s != '\r' && *s != '\n')
            return p;
        p = next;
    }
    return p;
}

/* =========================================================================
 * CTileDesc_ParseDat — main parser
 *
 * Reads ASCII .dat text content and populates *td.
 * Returns 1 on success, 0 if content is NULL or too short.
 *
 * Key parsing rules (from .dat format analysis):
 *   - Lines starting with // are comments (skip)
 *   - "-9" is a section separator (skip)
 *   - physical_occupancy and bitmap_occupancy are followed by dimension
 *     lines and then grid matrices
 *   - All other fields are single-line "KEY value..." format
 *   - Animation frame set table appears after the final -9 sentinel
 * ========================================================================= */

int CTileDesc_ParseDat(CTileDesc *td, const char *content, const char *filename) {
    if (!td || !content) return 0;

    memset(td, 0, sizeof(CTileDesc));
    td->rmb_seq_id      = -1;
    td->pickup_sound_id = -1;
    td->closed_fs       = -1;
    td->button_visible  = 1;
    for (int i = 0; i < TILE_MAX_EMPLOYEES; i++) td->possible_employees[i] = -1;
    for (int i = 0; i < TILE_MAX_MINIFIGS;  i++) td->possible_minifigs[i]  = -1;

    if (filename)
        snprintf(td->filename, sizeof(td->filename), "%s", filename);

    const char *p = content;
    char  line[512];
    char *tok[32];
    int   ntok;

    /* Track whether we've passed the final -9 separator (frame set table) */
    int past_sep = 0;
    int insert_easter_written = 0;
    int mobile_easter_written = 0;

    while (*p) {
        p = get_line(p, line, sizeof(line));

        /* Trim leading whitespace */
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;

        /* Skip empty lines and comments */
        if (!*s || s[0] == '/' || (s[0] == '\r' || s[0] == '\n'))
            continue;

        /* Section separator */
        if (s[0] == '-' && s[1] == '9') {
            past_sep = 1;
            continue;
        }

        ntok = tokenize(s, tok, 32);
        if (ntok == 0) continue;

        /* ----------------------------------------------------------------
         * physical_occupancy block
         * Format:
         *   physical_occupancy
         *   <blank line>
         *   cols rows [layers]
         *   <blank line>
         *   [layer 0 grid]
         *   <blank line>
         *   [layer 1 grid] ...
         * ---------------------------------------------------------------- */
        if (STRCASECMP(tok[0], "physical_occupancy") == 0) {
            /* Skip blank lines to reach dimension line */
            p = skip_blank(p);
            p = get_line(p, line, sizeof(line));
            int dims[3] = {1, 1, 1};
            parse_int_row(line, dims, 3);
            td->phys_cols   = (uint8_t)dims[0];
            td->phys_rows   = (uint8_t)dims[1];
            td->phys_layers = (uint8_t)dims[2];

            for (int lay = 0; lay < td->phys_layers && lay < TILE_MAX_PHYS_LAYERS; lay++) {
                int rows_read = 0;
                while (rows_read < td->phys_rows) {
                    p = get_line(p, line, sizeof(line));
                    char *ls = line;
                    while (*ls == ' ' || *ls == '\t') ls++;
                    if (!*ls) continue; /* blank line between layers */
                    int row_vals[TILE_MAX_PHYS_COLS] = {0};
                    int rc = parse_int_row(ls, row_vals, TILE_MAX_PHYS_COLS);
                    for (int c = 0; c < rc && c < td->phys_cols; c++)
                        td->phys_grid[lay][rows_read][c] = (uint8_t)row_vals[c];
                    rows_read++;
                }
            }
            continue;
        }

        /* ----------------------------------------------------------------
         * bitmap_occupancy block
         * ---------------------------------------------------------------- */
        if (STRCASECMP(tok[0], "bitmap_occupancy") == 0) {
            p = skip_blank(p);
            p = get_line(p, line, sizeof(line));
            int dims[2] = {1, 1};
            parse_int_row(line, dims, 2);
            td->bmp_cols = (uint8_t)dims[0];
            td->bmp_rows = (uint8_t)dims[1];

            int rows_read = 0;
            while (rows_read < td->bmp_rows) {
                p = get_line(p, line, sizeof(line));
                char *ls = line;
                while (*ls == ' ' || *ls == '\t') ls++;
                if (!*ls) continue;
                int row_vals[TILE_MAX_PHYS_COLS] = {0};
                int rc = parse_int_row(ls, row_vals, TILE_MAX_PHYS_COLS);
                for (int c = 0; c < rc && c < td->bmp_cols; c++)
                    td->bmp_grid[rows_read][c] = (uint8_t)row_vals[c];
                rows_read++;
            }
            continue;
        }

        /* ----------------------------------------------------------------
         * frame set table (after final -9 separator)
         * Lines: <name> <set_idx> <first_frame> <speed> <?> <delay_ms> <sound_id> <?>  <?>  <loop> <?>
         * ---------------------------------------------------------------- */
        if (past_sep && td->num_frame_sets > 0 && ntok >= 3) {
            /* Check if first non-name token looks like an integer */
            char *endp;
            strtol(tok[1], &endp, 10);
            if (endp > tok[1]) {
                int fs_idx = td->num_frame_sets;
                if (fs_idx < TILE_MAX_FRAME_SETS) {
                    AnimFrameSet *fs = &td->frame_sets[fs_idx];
                    snprintf(fs->name, sizeof(fs->name), "%s", tok[0]);
                    fs->set_idx    = (int16_t)parse_int_default(tok[1], 0);
                    fs->first_frame= (int16_t)parse_int_default(ntok > 2 ? tok[2] : NULL, 0);
                    fs->speed      = (int16_t)parse_int_default(ntok > 3 ? tok[3] : NULL, 1);
                    /* tok[4] = unknown */
                    fs->delay_ms   = (int16_t)parse_int_default(ntok > 5 ? tok[5] : NULL, 0);
                    fs->sound_id   = (int32_t)parse_int_default(ntok > 6 ? tok[6] : NULL, -1);
                    /* tok[7,8] = unknown */
                    fs->loop       = (int16_t)parse_int_default(ntok > 9 ? tok[9] : NULL, 0);
                    td->num_frame_sets++;
                }
                continue;
            }
        }

        /* ----------------------------------------------------------------
         * Single-line key-value fields
         * ---------------------------------------------------------------- */
        if (STRCASECMP(tok[0], "entry_exit") == 0 && ntok >= 5) {
            for (int i = 0; i < 4; i++)
                td->entry_exit[i] = parse_int_default(tok[1+i], 0);
        }
        else if (STRCASECMP(tok[0], "RMBSeq") == 0 && ntok >= 2) {
            td->rmb_seq_id = parse_int_default(tok[1], -1);
        }
        else if (STRCASECMP(tok[0], "LeisureDestination") == 0 && ntok >= 2) {
            td->leisure_dest = (uint8_t)parse_int_default(tok[1], 0);
        }
        else if (STRCASECMP(tok[0], "FreeToRoam") == 0 && ntok >= 5) {
            td->free_roam_x1 = (int16_t)parse_int_default(tok[1], 0);
            td->free_roam_y1 = (int16_t)parse_int_default(tok[2], 0);
            td->free_roam_x2 = (int16_t)parse_int_default(tok[3], 0);
            td->free_roam_y2 = (int16_t)parse_int_default(tok[4], 0);
        }
        else if (STRCASECMP(tok[0], "MaxEmployees") == 0 && ntok >= 2) {
            td->max_employees = (int8_t)parse_int_default(tok[1], 0);
        }
        else if (STRCASECMP(tok[0], "PossibleEmployees") == 0 && ntok >= 2) {
            for (int i = 0; i < TILE_MAX_EMPLOYEES && i+1 < ntok; i++)
                td->possible_employees[i] = parse_int_default(tok[1+i], -1);
        }
        else if (STRCASECMP(tok[0], "MaxMinifigForResource") == 0 && ntok >= 2) {
            td->max_minifig = (int8_t)parse_int_default(tok[1], 0);
        }
        else if (STRCASECMP(tok[0], "PossibleMinifigs") == 0 && ntok >= 2) {
            for (int i = 0; i < TILE_MAX_MINIFIGS && i+1 < ntok; i++)
                td->possible_minifigs[i] = parse_int_default(tok[1+i], -1);
        }
        else if (STRCASECMP(tok[0], "walk_speed") == 0 && ntok >= 3) {
            td->walk_speed_x = (int8_t)parse_int_default(tok[1], 1);
            td->walk_speed_y = (int8_t)parse_int_default(tok[2], 1);
        }
        else if (STRCASECMP(tok[0], "sex") == 0 && ntok >= 2) {
            td->sex = tok[1][0];
        }
        else if (STRCASECMP(tok[0], "PickUpSoundID") == 0 && ntok >= 2) {
            td->pickup_sound_id = parse_int_default(tok[1], -1);
        }
        else if (STRCASECMP(tok[0], "groundwidth") == 0 && ntok >= 2) {
            td->ground_width = (int16_t)parse_int_default(tok[1], 0);
        }
        else if (STRCASECMP(tok[0], "Shifts") == 0 && ntok >= 5) {
            for (int i = 0; i < 4; i++)
                td->shifts[i] = (int8_t)parse_int_default(tok[1+i], 0);
        }
        else if (STRCASECMP(tok[0], "ButtonVisible") == 0 && ntok >= 2) {
            td->button_visible = (uint8_t)parse_int_default(tok[1], 1);
        }
        else if (STRCASECMP(tok[0], "closedfs") == 0 && ntok >= 2) {
            td->closed_fs = (int8_t)parse_int_default(tok[1], -1);
        }
        else if (STRCASECMP(tok[0], "button") == 0
                 && ntok >= 2 && STRCASECMP(tok[1], "offset") == 0) {
            for (int i = 0; i < 3 && i+2 < ntok; i++)
                td->button_offset[i] = (int8_t)parse_int_default(tok[2+i], 0);
        }
        else if (STRCASECMP(tok[0], "Hotspot") == 0 && ntok >= 3) {
            td->hotspot_x = (int16_t)parse_int_default(tok[1], 0);
            td->hotspot_y = (int16_t)parse_int_default(tok[2], 0);
        }
        else if (STRCASECMP(tok[0], "total_number_of_frames") == 0 && ntok >= 2) {
            td->total_frames = (int16_t)parse_int_default(tok[1], 1);
        }
        else if (STRCASECMP(tok[0], "number_of_frame_sets") == 0 && ntok >= 2) {
            /* Store initial count as negative to mark "unread" — will be
             * incremented as frame set rows are parsed after -9 sentinel. */
            td->num_frame_sets = 0; /* reset; rows parsed incrementally above */
            /* Store target count for validation */
            /* (original count was tok[1]; we rebuild from rows) */
            (void)parse_int_default(tok[1], 1);
        }
        else if ((STRCASECMP(tok[0], "cursor/default_frame_set") == 0 ||
                  STRCASECMP(tok[0], "cursor_frame_set") == 0) && ntok >= 3) {
            td->cursor_fs_idx = (int16_t)parse_int_default(tok[1], 0);
            td->cursor_fs_n   = (int16_t)parse_int_default(tok[2], 0);
        }
        else if (STRCASECMP(tok[0], "InsertSeq") == 0 && ntok >= 3) {
            td->insert_seq_group = parse_int_default(tok[1], 0);
            td->insert_seq_id    = parse_int_default(tok[2], 0);
            insert_easter_written = 0;
        }
        else if (STRCASECMP(tok[0], "MobileSeq") == 0 && ntok >= 3) {
            td->mobile_seq_group = parse_int_default(tok[1], 0);
            td->mobile_seq_id    = parse_int_default(tok[2], 0);
            mobile_easter_written = 0;
        }
        else if (STRCASECMP(tok[0], "EasterEgg") == 0) {
            /* First EasterEgg after InsertSeq; second after MobileSeq */
            if (!insert_easter_written) {
                snprintf(td->insert_easter_egg, sizeof(td->insert_easter_egg), "%s", s);
                insert_easter_written = 1;
            } else {
                snprintf(td->mobile_easter_egg, sizeof(td->mobile_easter_egg), "%s", s);
                mobile_easter_written = 1;
            }
        }
    }

    return 1;
}

/* =========================================================================
 * CTileDesc_IsConnected
 * Returns 1 if this tile has a road/track connection on the given side.
 * entry_exit > 0 means connected (0 = not connected).
 * ========================================================================= */

int CTileDesc_IsConnected(const CTileDesc *td, int side) {
    if (!td || side < 0 || side > 3) return 0;
    return (td->entry_exit[side] > 0) ? 1 : 0;
}

/* =========================================================================
 * CTileDesc_Lookup
 * Linear search through global tile descriptor table by resource_id.
 * Original: CGameWorld uses a hash map or sorted array; linear here is fine
 * for startup lookup (627 entries max).
 * ========================================================================= */

CTileDesc *CTileDesc_Lookup(uint32_t resource_id) {
    for (int i = 0; i < g_tileDescCount; i++) {
        if (g_tileDescs[i].resource_id == resource_id)
            return &g_tileDescs[i];
    }
    return NULL;
}
