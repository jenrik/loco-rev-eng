/**
 * wave_io.c — RIFF/WAVE file loading helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Helper functions for loading RIFF/WAVE audio files. The primary entry
 * point is Game_LoadWaveFile, which opens a WAVE file via the asset
 * manager or filesystem, parses its RIFF structure, and fills a
 * WaveLoadBuf struct with format and PCM data.
 *
 * WaveLoadBuf layout (at param_2, 0x20 bytes):
 *   +0x00: (unknown / intermediate)
 *   +0x04: audio format field
 *   +0x18: data_size (uint32, PCM data byte count)
 *   +0x1C: data_buffer (void*, allocated PCM data)
 *
 * Called by: RESMGR_LoadSoundResource (0x448DC1)
 */

#include <stdint.h>

/* Stream object (WNDPROC stream) */
typedef struct WNDPROC_Stream {
    int*   vtable;         /* +0x00: function table pointer */
    int    position;       /* +0x04: current read/write position */
    int    size;           /* +0x08: total data size */
    /* ... additional fields ... */
} WNDPROC_Stream;

/* RIFF chunk header (12 bytes) */
typedef struct {
    uint32_t chunk_id;     /* 4 bytes: e.g. "RIFF", "fmt ", "data" */
    uint32_t chunk_size;   /* 4 bytes: size of chunk data */
    uint32_t format;       /* 4 bytes: RIFF format type (e.g. "WAVE") */
} RiffChunkHeader;

/**
 * Game_ReadChunk — Read or search for a RIFF chunk in a stream.
 * Address: 0x413980
 *
 * Two modes based on param_4:
 *   mode=0x00: Read 12 bytes from stream into chunk_header (3 x uint32).
 *   mode=0x10: Search for sub-chunk matching *chunk_header->chunk_id.
 *              Reads 8-byte headers, seeks past non-matching chunks.
 *
 * @param stream       Pointer to stream object
 * @param chunk_header Pointer to 12-byte chunk header buffer
 *                     (on input for mode 0x10: search target)
 * @param param_3      Unused parameter (0 always)
 * @param flags        0x00 = raw read, 0x10 = search by ID
 * @return 0 on success, 0xFFFFFFFF if null stream, 0x109 on error/not-found
 */
int __cdecl Game_ReadChunk(WNDPROC_Stream* stream, RiffChunkHeader* chunk_header,
                           int param_3, int flags)
{
    if (stream == NULL) {
        return 0xFFFFFFFF;
    }

    if (flags == 0x10) {
        /* Search mode: iterate sub-chunks until ID matches */
        RiffChunkHeader search_target = *chunk_header;  /* save the ID we're looking for */

        extern void __thiscall Stream_BeginEnum(WNDPROC_Stream* stream);
        extern int  __thiscall WIN32_StreamRead(WNDPROC_Stream* stream, void* buf, int size);
        extern int  __thiscall WNDPROC_StreamSeekForward(WNDPROC_Stream* stream, int a, int offset, int b);
        extern void __thiscall WNDPROC_EnterCriticalSection(void* cs);
        extern void __thiscall WNDPROC_LeaveCriticalSection(void* cs);

        Stream_BeginEnum(stream);

        /* Read first chunk header */
        RiffChunkHeader cur_header;
        WIN32_StreamRead(stream, &cur_header, 8);

        while (cur_header.chunk_id != search_target.chunk_id) {
            int chunk_data_size = cur_header.chunk_size;

            /* Check if stream is at end (flag & 1) */
            void* vt = *(void**)stream;
            int flags_at = *(int*)((uint8_t*)stream + *(int*)((uint8_t*)vt + 4) + 8);
            if (flags_at & 1) break;

            /* Lock critical section if active */
            int sync_active = *(int*)((uint8_t*)stream + 0x34);
            if (sync_active < 0) {
                WNDPROC_EnterCriticalSection((uint8_t*)stream + 0x38);
            }

            stream->position++;

            /* Seek past chunk data to next header */
            WNDPROC_StreamSeekForward(stream, 0, chunk_data_size + 1, -1);

            /* Unlock critical section if active */
            if (sync_active < 0) {
                WNDPROC_LeaveCriticalSection((uint8_t*)stream + 0x38);
            }

            WIN32_StreamRead(stream, &cur_header, 8);
        }

        if (cur_header.chunk_id != search_target.chunk_id) {
            return 0x109;  /* not found */
        }

        /* Copy the found header back to caller */
        *chunk_header = cur_header;
        return 0;
    }

    if (flags == 0x20 || flags == 0x40) {
        /* These modes are not handled (would fall through to return 0) */
        return 0;
    }

    /* Mode 0x00: raw read of 12 bytes into chunk header */
    {
        extern void __thiscall Stream_BeginRead(WNDPROC_Stream* stream, int a, int b);
        Stream_BeginRead(stream, 0, 0);

        extern int __thiscall WIN32_StreamRead(WNDPROC_Stream* stream, void* buf, int size);
        WIN32_StreamRead(stream, chunk_header, 12);

        if (stream->size != 12) {
            return 0x109;  /* read error */
        }
    }

    return 0;
}


/**
 * Game_LoadWaveFile — Load a RIFF/WAVE file into a buffer struct.
 * Address: 0x413660
 *
 * Opens a WAVE file path via the asset manager or filesystem, validates
 * the RIFF "WAVE" header, parses the "fmt " chunk, reads format data
 * into WaveLoadBuf+0x04, then reads the "data" chunk into an allocated
 * buffer at WaveLoadBuf+0x1C (size at +0x18).
 *
 * Uses SEH for stream cleanup on error.
 *
 * @param path    Path to the .wav file
 * @param out_buf Pointer to WaveLoadBuf struct (0x20 bytes, output)
 * @return 0 on success, 0xFFFFFFFF on failure, may call CRT_exit on error.
 *   Binary error codes: 0xe102 (read/IO error), 0xe101 (format error),
 *   0xe000 (allocation error). Source uses descriptive strings instead.
 */
int __cdecl Game_LoadWaveFile(const char* path, void* out_buf)
{
    extern void* g_asset_mgr;       /* 0x4AA5B0 — asset manager */
    extern char  g_install_path[];  /* 0x4A99C8 */
    extern void* __cdecl operator_new(size_t size);
    extern void  __cdecl CRT_free(void* ptr);
    extern void  __cdecl CRT_exit(void* stack, const char* msg);
    extern int* __thiscall AssetMgr_LoadFile(void* mgr, const char* path, int* out_size);
    extern WNDPROC_Stream* __thiscall WNDPROC_StreamFromMemory(void* stream, const char* data, int size, int mode);
    extern WNDPROC_Stream* __thiscall WIN32_StreamOpen(void* stream, int mode);
    extern int   __thiscall WIN32_StreamOpenPath(void* stream, const char* path, int flags, const char* mode);
    extern int   __thiscall WIN32_StreamRead(void* stream, void* buf, int size);
    extern void* __cdecl CRT_malloc(size_t size);

    WNDPROC_Stream* stream = NULL;
    int*   asset_data = NULL;
    int    data_size = 0;
    int    stream_result;
    RiffChunkHeader chunk_hdr;
    uint32_t fmt_chunk_size;

    /* Step 1: Try asset manager first */
    if (g_asset_mgr != NULL) {
        /* Adjust path to strip install path prefix.
         * Binary computes: path + (strlen - 1). The -1 skips the
         * trailing separator, yielding the relative path within. */
        int prefix_len = 0;
        while (g_install_path[prefix_len]) { prefix_len++; }
        const char* rel_path = path + prefix_len;

        asset_data = AssetMgr_LoadFile(&g_asset_mgr, rel_path, &data_size);

        if (asset_data != NULL) {
            WNDPROC_Stream* stream_mem = (WNDPROC_Stream*)operator_new(0x5C);
            if (stream_mem != NULL) {
                stream = WNDPROC_StreamFromMemory(stream_mem, (const char*)asset_data, data_size, 1);
            }
        }
    }

    /* Step 2: Fall back to direct file open */
    if (stream == NULL) {
        WNDPROC_Stream* stream_file = (WNDPROC_Stream*)operator_new(0x5C);
        if (stream_file != NULL) {
            extern void* DAT_00479190;  /* read-mode string */
            WIN32_StreamOpen(stream_file, 1);
            WIN32_StreamOpenPath(stream_file, path, 0xA1, (const char*)&DAT_00479190);

            /* Check if stream opened successfully (flag test) */
            void* vt = *(void**)stream_file;
            int flags = *(int*)((uint8_t*)stream_file + *(int*)((uint8_t*)vt + 4) + 0x4C);
            if (flags != -1) {
                stream = stream_file;
            }
        }
    }

    if (stream == NULL) {
        return 0xFFFFFFFF;  /* failed to open */
    }

    /* Step 3: Read RIFF header */
    chunk_hdr.chunk_id = 0;
    stream_result = Game_ReadChunk(stream, &chunk_hdr, 0, 0);
    if (stream_result != 0) {
        CRT_exit(&stream_result, "wave_io.c: RIFF header read failed");
    }

    /* Validate RIFF/WAVE signature */
    if (chunk_hdr.chunk_id != 0x46464952 ||  /* "RIFF" */
        chunk_hdr.format != 0x45564157)       /* "WAVE" */
    {
        CRT_exit(&stream_result, "wave_io.c: not a WAVE file");
    }

    /* Step 4: Read "fmt " chunk */
    chunk_hdr.chunk_id = 0x20746D66;  /* "fmt " */
    stream_result = Game_ReadChunk(stream, &chunk_hdr, 0, 0x10);
    if (stream_result != 0) {
        CRT_exit(&stream_result, "wave_io.c: fmt chunk search failed");
    }

    if (chunk_hdr.chunk_size > 18) {
        CRT_exit(&stream_result, "wave_io.c: fmt chunk too large");
    }

    /* Read format data into out_buf+4 */
    {
        uint32_t fmt_size = chunk_hdr.chunk_size;
        uint8_t* fmt_dest = (uint8_t*)out_buf + 4;
        uint8_t* fmt_ptr = fmt_dest;

        int read_ok = 0;
        if (fmt_size > 0) {
            // Read the format data from stream
            WIN32_StreamRead(stream, fmt_dest, fmt_size);
            read_ok = (int)((WNDPROC_Stream*)stream)->size >= (int)fmt_size;
        }

        if (!read_ok) {
            CRT_exit(&stream_result, "wave_io.c: fmt data read failed");
        }
    }

    /* Step 5: Read "data" chunk */
    chunk_hdr.chunk_id = 0x61746164;  /* "data" */
    stream_result = Game_ReadChunk(stream, &chunk_hdr, 0, 0x10);
    if (stream_result != 0) {
        CRT_exit(&stream_result, "wave_io.c: data chunk search failed");
    }

    /* Allocate buffer for PCM data */
    *(uint32_t*)((uint8_t*)out_buf + 0x18) = chunk_hdr.chunk_size;  /* data_size */

    void* pcm_data = CRT_malloc(chunk_hdr.chunk_size + 1);
    *(void**)((uint8_t*)out_buf + 0x1C) = pcm_data;

    if (pcm_data == NULL) {
        CRT_exit(&stream_result, "wave_io.c: PCM buffer alloc failed");
    }

    /* Read PCM data (with error check) */
    {
        uint8_t* pcm_ptr = (uint8_t*)pcm_data;
        uint32_t to_read = chunk_hdr.chunk_size;

        WIN32_StreamRead(stream, pcm_ptr, to_read);
        if ((int)((WNDPROC_Stream*)stream)->size < (int)to_read) {
            CRT_exit(&stream_result, "wave_io.c: PCM data read failed");
        }
    }

    /* Step 6: Cleanup — matches binary tail at 0x41391D.
     * Binary sequence:
     *   1. Delete memory stream (local_18) via vtable[0] with flags=1
     *   2. Free asset buffer (local_1c) via CRT_free
     *   3. WIN32_StreamDestroyImmediate on file stream (local_20)
     *   4. Delete file stream (local_20) via vtable[0] with flags=1
     *
     * TODO (ISS-raw-115-07): File-stream path (steps 3-4) not yet
     * tracked separately from memory-stream path. */
    if (stream != NULL) {
        /* Destroy stream via its vtable[0] scalar deleting destructor */
        void* vtable = *(void**)stream;
        void (**dtor)(void*, int) = (void(**)(void*, int))vtable;
        dtor[0](stream, 1);
    }

    if (asset_data != NULL) {
        CRT_free(asset_data);
    }

    return 0;
}
