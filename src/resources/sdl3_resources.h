/*
 * sdl3_resources.h — Resource loader for .RFH/.RFD archive files
 *
 * Replaces: Resource loading in loco.exe at 0x00446050-0x0045caa0
 *
 * The original game stores assets in a pair of files:
 *   resource.RFH — index: (filenameLen, filename, rfdOffset, rfdSize) entries
 *   resource.RFD — data: raw concatenated asset blobs
 *
 * Path read from lego.ini [DIRECTORIES] ResFile=.
 */

#ifndef LOCO_SDL3_RESOURCES_H
#define LOCO_SDL3_RESOURCES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque resource manager handle. */
typedef struct ResMgr ResMgr;

/** One entry in the RFH index. */
typedef struct {
    char     filename[128];  /* Asset path relative to data dir        */
    uint32_t rfd_offset;     /* Byte offset in the .RFD file           */
    uint32_t rfd_size;       /* Byte size of the asset                 */
} ResEntry;

/**
 * ResMgr_Open — Open resource.RFH and resource.RFD.
 *
 * @param   data_dir  Path to game data directory (contains resource.RFH).
 * @return  Handle, or NULL on failure.
 */
ResMgr *ResMgr_Open(const char *data_dir);

/**
 * ResMgr_Close — Close files and free memory.
 */
void ResMgr_Close(ResMgr *mgr);

/**
 * ResMgr_GetEntryCount — Number of entries in the RFH index.
 */
int ResMgr_GetEntryCount(const ResMgr *mgr);

/**
 * ResMgr_GetEntry — Get an entry by index.
 */
const ResEntry *ResMgr_GetEntry(const ResMgr *mgr, int index);

/**
 * ResMgr_FindEntry — Find an entry by filename (case-insensitive).
 */
const ResEntry *ResMgr_FindEntry(const ResMgr *mgr, const char *filename);

/**
 * ResMgr_LoadEntry — Load an entry's data into a newly allocated buffer.
 *
 * @param   mgr      Resource manager.
 * @param   entry    The entry to load.
 * @param   out_size [out] Size of the loaded data.
 * @return  Allocated buffer (caller must free()), or NULL on failure.
 */
void *ResMgr_LoadEntry(const ResMgr *mgr, const ResEntry *entry,
                       size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_SDL3_RESOURCES_H */
