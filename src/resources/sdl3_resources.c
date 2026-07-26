/*
 * sdl3_resources.c — Resource loader for .RFH/.RFD archive files
 *
 * RFH FORMAT (binary, little-endian):
 *   Repeated until EOF:
 *     uint32_t  filenameLen       — byte length of filename
 *     char      filename[filenameLen]  — null-terminated path
 *     uint32_t  rfdOffset         — byte offset in .RFD
 *     uint32_t  rfdSize           — byte size in .RFD
 */

#include "sdl3_resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define RFH_MAX_ENTRIES  4096
#define MAX_PATH_LEN     512

struct ResMgr {
    FILE     *rfd_file;       /* Open handle to resource.RFD          */
    ResEntry  entries[RFH_MAX_ENTRIES];
    int       entry_count;
};

ResMgr *ResMgr_Open(const char *data_dir)
{
    if (!data_dir) return NULL;

    /* Build paths to resource.RFH and resource.RFD.
     * Original LEGO.INI: ResFile=d:\loco\art-res\resource.rfh
     * We try multiple locations relative to the data directory. */
    const char *search_dirs[] = {
        "",           /* data_dir itself */
        "art-res/",   /* unpacked game structure */
        "Exe/",       /* Windows game structure */
        "../art-res/",/* data_dir=Exe/, art-res is sibling */
        NULL
    };

    char rfh_path[MAX_PATH_LEN];
    char rfd_path[MAX_PATH_LEN];
    FILE *rfh = NULL;
    FILE *rfd = NULL;

    for (int i = 0; search_dirs[i]; i++) {
        snprintf(rfh_path, sizeof(rfh_path), "%s%sresource.RFH",
                 data_dir, search_dirs[i]);
        rfh = fopen(rfh_path, "rb");
        if (rfh) {
            snprintf(rfd_path, sizeof(rfd_path), "%s%sresource.RFD",
                     data_dir, search_dirs[i]);
            rfd = fopen(rfd_path, "rb");
            if (rfd) break;
            fclose(rfh);
            rfh = NULL;
        }
        /* Also try Resource.RFH (capitalized) */
        snprintf(rfh_path, sizeof(rfh_path), "%s%sResource.RFH",
                 data_dir, search_dirs[i]);
        rfh = fopen(rfh_path, "rb");
        if (rfh) {
            snprintf(rfd_path, sizeof(rfd_path), "%s%sResource.RFD",
                     data_dir, search_dirs[i]);
            rfd = fopen(rfd_path, "rb");
            if (rfd) break;
            fclose(rfh);
            rfh = NULL;
        }
    }

    if (!rfh || !rfd) {
        if (rfh) fclose(rfh);
        if (rfd) fclose(rfd);
        return NULL;
    }

    ResMgr *mgr = (ResMgr*)calloc(1, sizeof(ResMgr));
    if (!mgr) { fclose(rfh); fclose(rfd); return NULL; }

    mgr->rfd_file = rfd;

    /* Parse RFH entries */
    int count = 0;
    while (count < RFH_MAX_ENTRIES) {
        uint32_t filename_len = 0;
        if (fread(&filename_len, 4, 1, rfh) != 1) break;

        /* Sanity check */
        if (filename_len == 0 || filename_len >= sizeof(mgr->entries[count].filename)) {
            break;
        }

        /* Read filename */
        if (fread(mgr->entries[count].filename, filename_len, 1, rfh) != 1) break;
        mgr->entries[count].filename[filename_len] = '\0';

        /* Read offset and size */
        if (fread(&mgr->entries[count].rfd_offset, 4, 1, rfh) != 1) break;
        if (fread(&mgr->entries[count].rfd_size, 4, 1, rfh) != 1) break;

        count++;
    }

    mgr->entry_count = count;
    fclose(rfh);
    return mgr;
}

void ResMgr_Close(ResMgr *mgr)
{
    if (mgr) {
        if (mgr->rfd_file) fclose(mgr->rfd_file);
        free(mgr);
    }
}

int ResMgr_GetEntryCount(const ResMgr *mgr)
{
    return mgr ? mgr->entry_count : 0;
}

const ResEntry *ResMgr_GetEntry(const ResMgr *mgr, int index)
{
    if (!mgr || index < 0 || index >= mgr->entry_count) return NULL;
    return &mgr->entries[index];
}

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

const ResEntry *ResMgr_FindEntry(const ResMgr *mgr, const char *filename)
{
    if (!mgr || !filename) return NULL;
    for (int i = 0; i < mgr->entry_count; i++) {
        if (strcasecmp_simple(mgr->entries[i].filename, filename) == 0) {
            return &mgr->entries[i];
        }
    }
    return NULL;
}

void *ResMgr_LoadEntry(const ResMgr *mgr, const ResEntry *entry,
                       size_t *out_size)
{
    if (!mgr || !entry || !mgr->rfd_file) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    void *data = malloc(entry->rfd_size);
    if (!data) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    if (fseek(mgr->rfd_file, (long)entry->rfd_offset, SEEK_SET) != 0) {
        free(data);
        if (out_size) *out_size = 0;
        return NULL;
    }

    size_t read = fread(data, 1, entry->rfd_size, mgr->rfd_file);
    if (read != entry->rfd_size) {
        free(data);
        if (out_size) *out_size = 0;
        return NULL;
    }

    if (out_size) *out_size = entry->rfd_size;
    return data;
}
