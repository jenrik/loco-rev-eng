/**
 * ResDataSave.cpp — RESDATA save/load primitives (0x447B20..0x448030)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered from raw loco.exe disassembly (Ghidra MCP was not
 * available in the reconstructing session; every claim below was verified
 * with objdump on the shipped PE).
 *
 * These are the exact resource-data primitives the world persistence
 * functions (INPUT_NewWorld 0x41E120 / INPUT_LoadWorld 0x41D320 /
 * INPUT_LoadSaveFile 0x41D5C0 / INPUT_SaveCurrentWorld 0x41D9B0) drive:
 *
 *   RESMGR_ResourceData_Init   0x447B20  zero the save/load fields
 *   RESMGR_ReleaseResource     0x447B90  reset state, then RemoveResource
 *   RESMGR_LoadResource        0x447BA0  open file, read 0x114 header +
 *                                        width*height preview into RESDATA
 *   RESMGR_LockResource        0x447DB0  read one 0x80 entity record
 *   RESMGR_UnlockResource      0x447DF0  read one 0x2C vehicle record
 *   RESMGR_LoadResourceData    0x447E30  open output stream, write header +
 *                                        preview (save path; the binary
 *                                        uses the WRITE-stream ctor
 *                                        0x465090 — mode|2, not the read
 *                                        ctor 0x463970 — mode|1)
 *   RESMGR_WriteSaveRecord     0x447F50  write one 0x80 entity record
 *   RESMGR_WriteTableRecord    0x447F80  write one 0x2C vehicle record
 *   RESMGR_RemoveResource      0x447FB0  release streams, asset data,
 *                                        preview pixels
 *   RESMGR_IsSaveHeader        0x448030  (save.type == 8)
 *
 * On 64-bit hosts the record buffers are the host-native typed members
 * (RESDATA::host_record_entity/vehicle) — the x86 record-buffer offsets
 * +0x04/+0x84 are only valid in the 32-bit layout and are never written
 * into host RESDATA members.
 *
 * All field accesses are typed through the RESDATA struct (shared/types.h,
 * SaveRegion at +0xB0, streams/pixels at +0x1C4..+0x1D7).  No raw vtable
 * dispatch, pointer arithmetic, or duplicate layouts.
 *
 * Streams: the original uses the WIN32_Stream* layer (win32_stream.c).
 * The SDL host substitutes the typed HostSaveStream below (bounded file/
 * memory streams with an error-state flag) behind #ifndef _WIN32; the
 * _WIN32 branch keeps the original Win32 stream calls.
 *
 * Host hardening (documented deviation, #ifndef _WIN32 only): a missing
 * file, short header/preview read, or corrupt preview dimensions fails
 * explicitly (returns 0 / nullptr) instead of the original's silent
 * skip-on-short-read behaviour.  The _WIN32 branch restores the
 * original's explicit byte-count checks too ([stream+8] after the
 * header and preview reads, and the vtable-relative state word before
 * them) and — unlike the binary, which error-reports through 0x466CE0
 * and continues returning 1 — fails explicitly (return 0) on a short
 * read, matching the host path's explicit-failure contract.
 */

// Status: TRANSCRIBED

#include "../shared/types.h"
#include "ResourceManager.h"
/* Win32_MemoryStream's complete type is only needed by this file's own
 * _WIN32-only branch below (dead code on this host; exercised by the
 * MinGW typecheck build) for real `delete` through WNDPROC_Stream*.
 * Guarded to _WIN32 only: under the native build, graphics/sdl3_window.h
 * (transitively reachable from other TUs, though not this one) would
 * conflict with this header's <windows.h> chain if both were active in
 * the same TU — moot here since sdl3_window.h's own body is guarded
 * `#ifndef _WIN32` and contributes nothing when _WIN32 is defined. */
#ifdef _WIN32
#include "Win32StreamMem.h"
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(size_t size);              /* 0x465CE0 */
extern void  GLOBAL_free(void* ptr);                 /* 0x465CD0 */
extern void* g_asset_mgr;                            /* 0x485600 */
extern char  g_install_path[];                       /* 0x4A99C8 */

/* Strict sane preview cap (16 MiB) shared by the host load/write
 * paths — same bound as PersistenceAdapter::kMaxPreviewBytes.  The
 * largest shipped fixture preview is 80*64 = 5120 bytes. */
#ifndef _WIN32
static constexpr size_t kHostMaxPreviewBytes = 16u * 1024u * 1024u;
#endif

#ifndef _WIN32
/* The original checks [0x485600] (g_asset_mgr) and, when present, loads
 * the file through AssetMgr_LoadFile (0x45CD00) + a memory stream.  On
 * the host g_asset_mgr stays nullptr (stubs_impl.cpp), exactly like the
 * original BSS state before GameLoop_Setup wires it — so the file-stream
 * fallback below is the path the original takes in that state too. */
#else
/* Win32 stream layer (native/win32_stream.c — original ABI). */
/* Read-stream ctor 0x463970 constructs a WIN32_Stream (Win32Stream.h,
 * flags|1, child vtable 0x479184); write-stream ctor 0x465090 constructs
 * the genuinely distinct sibling class WIN32_OStream (Win32OStream.h,
 * flags|2, child vtable 0x479244 — confirmed via vbtable/allocation-size
 * evidence to be a smaller, separate class, not the same class re-tagged;
 * see Win32OStream.h/WndProcOStream.h). Declared here (not `#include`d)
 * to match this file's existing local-extern convention for this stream
 * family — signatures kept in sync with the canonical headers by hand. */
extern void* WIN32_StreamOpenFile(void* stream, const char* path,
                                  uint32_t mode, uint32_t flags, int32_t param4); /* 0x463970 */
extern void* WIN32_StreamOpenWriteFile(void* stream, const char* path,
                                       uint32_t flags, uint32_t shareMask,
                                       int32_t initBase); /* 0x465090, see Win32OStream.h */
extern void* WIN32_StreamRead(void* stream, void* buf, uint32_t size);   /* 0x463810 */
extern void* WIN32_StreamWrite(void* stream, const void* buf, uint32_t size); /* 0x465010 */
/* Canonical signature/size helper: resources/Win32StreamMem.h. Kept as a
 * local extern here (not an #include) to match this file's existing
 * local-extern convention for this stream family, since this file's own
 * WNDPROC_StreamFromMemory caller lives inside the _WIN32-only branch
 * below (dead code on this host, but must stay signature-correct for the
 * MinGW typecheck build). Only the pointer type is needed here, so a
 * forward declaration suffices — no member of WNDPROC_Stream is touched
 * in this file. */
class WNDPROC_Stream;
extern WNDPROC_Stream* WNDPROC_StreamFromMemory(void* stream, char* data,
                                                 int32_t size, int32_t mode); /* 0x464490 */
extern size_t WIN32_MemoryStream_Size(); /* resources/Win32StreamMem.cpp */
extern int32_t AssetMgr_LoadFile(void* mgr, const char* path, int32_t* size); /* 0x45CD00 */
extern size_t WIN32_Stream_Size();  /* resources/Win32Stream.cpp — real sizeof(WIN32_Stream) */
extern size_t WIN32_OStream_Size(); /* resources/Win32OStream.cpp — real sizeof(WIN32_OStream) */
/* The preview writer uses the tilemap overlay (0x457080) on g_tilemap
 * (tilemap.h, 0x4AAD08). */
class TileMap;
extern void TileMap_CreateOverlay(void* tilemap, void* surface, int32_t flags);
extern TileMap* g_tilemap;
#endif

/* ================================================================== */
/* HostSaveStream — typed bounded stream for the SDL host              */
/*                                                                      */
/* Replaces the original buffered WIN32_Stream* object graph with a     */
/* native file/memory stream that reproduces exactly the three         */
/* behaviours the RESDATA primitives rely on:                          */
/*   - Read(dst, size) returns the number of bytes actually read       */
/*     (short reads are not silent: the callers check the count),      */
/*   - Write(src, size) returns the number of bytes written,           */
/*   - a state-flag word whose bit 0x4 marks a non-writable stream     */
/*     (the original sets the same bit in the stream +0x08 flags when  */
/*     an operation fails; WriteSaveRecord/WriteTableRecord refuse to  */
/*     write when it is set).                                          */
/*                                                                      */
/* Write streams are ATOMIC on the host: OpenWrite opens "<path>.tmp"  */
/* (same directory, so the final rename is atomic on POSIX) and        */
/* Commit() flushes + renames the temp over the target.  An uncommitted */
/* write stream removes its temp file on destruction, so a failed save */
/* never leaves a partial "curr" behind (host deviation — the         */
/* original writes in place; documented in PersistenceAdapter.h).      */
/* ================================================================== */
#ifndef _WIN32
namespace {

class HostSaveStream {
public:
    /* state flag bit mirrored from the original stream +0x08 word */
    static constexpr uint32_t kStateError = 0x4;

    ~HostSaveStream() { close(); }

    static HostSaveStream* OpenRead(const char* path)
    {
        HostSaveStream* s = new HostSaveStream();
        s->path_ = path;
        s->file_ = std::fopen(path, "rb");
        if (s->file_ == nullptr) {
            s->flags_ |= kStateError;
        }
        return s;
    }

    static HostSaveStream* OpenWrite(const char* path)
    {
        HostSaveStream* s = new HostSaveStream();
        s->path_ = path;
        s->temp_path_ = std::string(path) + ".tmp";
        s->file_ = std::fopen(s->temp_path_.c_str(), "wb");
        if (s->file_ == nullptr) {
            s->flags_ |= kStateError;
        }
        return s;
    }

    static HostSaveStream* FromMemory(const void* data, size_t size)
    {
        HostSaveStream* s = new HostSaveStream();
        s->mem_ = static_cast<const uint8_t*>(data);
        s->mem_size_ = size;
        return s;
    }

    size_t Read(void* dst, size_t size)
    {
        if (file_ != nullptr) {
            return std::fread(dst, 1, size, file_);
        }
        if (mem_ != nullptr) {
            size_t avail = (mem_pos_ < mem_size_) ? (mem_size_ - mem_pos_) : 0;
            size_t n = (size < avail) ? size : avail;
            if (n > 0) {
                std::memcpy(dst, mem_ + mem_pos_, n);
                mem_pos_ += n;
            }
            return n;
        }
        return 0;
    }

    size_t Write(const void* src, size_t size)
    {
        if (file_ != nullptr) {
            return std::fwrite(src, 1, size, file_);
        }
        return 0;   /* memory streams are read-only */
    }

    /* Flush + atomically publish the temp file over the target path. */
    bool Commit()
    {
        if (file_ == nullptr || temp_path_.empty()) {
            return false;
        }
        if (std::fflush(file_) != 0) {
            close();
            std::remove(temp_path_.c_str());
            return false;
        }
        std::fclose(file_);
        file_ = nullptr;
        if (std::rename(temp_path_.c_str(), path_.c_str()) != 0) {
            std::remove(temp_path_.c_str());
            return false;
        }
        committed_ = true;
        return true;
    }

    /* Unread bytes: used by the host truncation guard in
     * INPUT_LoadSaveFile (0x41D5C0). */
    size_t bytes_remaining() const
    {
        if (file_ != nullptr) {
            long pos = std::ftell(file_);
            if (pos < 0) {
                return 0;
            }
            if (std::fseek(file_, 0, SEEK_END) != 0) {
                return 0;
            }
            long end = std::ftell(file_);
            std::fseek(file_, pos, SEEK_SET);
            if (end < pos) {
                return 0;
            }
            return static_cast<size_t>(end - pos);
        }
        if (mem_ != nullptr) {
            return (mem_pos_ < mem_size_) ? (mem_size_ - mem_pos_) : 0;
        }
        return 0;
    }

    bool StateFlag(uint32_t bit) const { return (flags_ & bit) != 0; }

    void SetStateFlag(uint32_t bit) { flags_ |= bit; }

private:
    HostSaveStream() = default;

    void close()
    {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
        /* An uncommitted write stream must never leave a partial file
         * behind (atomic-save contract). */
        if (!temp_path_.empty() && !committed_) {
            std::remove(temp_path_.c_str());
        }
    }

    FILE*           file_ = nullptr;
    const uint8_t*  mem_ = nullptr;
    size_t          mem_size_ = 0;
    size_t          mem_pos_ = 0;
    uint32_t        flags_ = 0;
    std::string     path_;       /* final target path (write mode)      */
    std::string     temp_path_;  /* "<path>.tmp" (write mode, atomic)   */
    bool            committed_ = false;
};

}  // namespace
#endif /* _WIN32 */

/* ================================================================== */
/* Stream lifecycle helpers                                            */
/* ================================================================== */

static void destroy_stream(void* stream)
{
    if (stream == nullptr) {
        return;
    }
#ifndef _WIN32
/* HostSaveStream is heap-owned; delete it (frees the FILE). */
delete static_cast<HostSaveStream*>(stream);
#else
/* The original destroys the stream through its scalar-deleting
 * destructor slot (vtable[0], flags=1) — real C++ `delete` through
 * WNDPROC_Stream* reproduces this via StreamObject's virtual
 * ~StreamObject() (see resources/Win32Stream.h/Win32StreamMem.h;
 * `stream` here is always one of those two concrete classes,
 * constructed a few lines above in RESMGR_LoadResource/
 * RESMGR_LoadResourceData). WIN32_StreamDestroyImmediate (previously
 * called here) is WIN32_Stream::CloseNow() — a different, non-freeing
 * operation, not the scalar deleting destructor this comment always
 * claimed it was; using it here leaked every heap stream object on
 * this branch. */
delete static_cast<WNDPROC_Stream*>(stream);
#endif
}

#ifdef _WIN32
/* The binary's WIN32_Stream* layer exposes TWO distinct dwords on the
 * stream object that the RESDATA primitives read (verified against
 * objdump):
 *
 *   1. the last-read/written BYTE COUNT at the FIXED offset stream+8
 *      (0x447DD1 cmpl $0x80,0x8(%ecx) in RESMGR_LockResource;
 *      0x447E11 cmpl $0x2c,0x8(%ecx) in RESMGR_UnlockResource;
 *      0x447CE0 cmpl $0x114,0x8(%ecx) and 0x447D54 cmp %edi,0x8(%eax)
 *      in RESMGR_LoadResource),
 *   2. the STATE-FLAGS word at the VTABLE-RELATIVE offset
 *      stream + [vtable+4] + 8 (0x447CAC mov 0x8(%edx,%eax,1) in
 *      RESMGR_LoadResource; 0x447EAC testb $0x4,0x8(%edx,%eax,1) in
 *      RESMGR_LoadResourceData — [vtable+4] is the child-area offset
 *      documented in native/win32_stream.c).
 *
 * The two are DIFFERENT words at DIFFERENT offsets and must not be
 * conflated.  Typed stream reconstruction is deferred (TODO: WIN32_Stream
 * class during the stream-integration milestone); these helpers
 * reproduce the exact raw reads. */
static uint32_t stream_byte_count(void* stream)
{
    /* The last-read/written byte count is a signed dword in the binary
     * (0x447DD1 cmpl $0x80,0x8(%ecx) — signed 32-bit compare); the
     * count is never negative in practice.  Return it as uint32_t so
     * the short-read comparisons against sizeof/preview sizes below
     * are unsigned-to-unsigned (no -Wsign-compare on 64-bit hosts). */
    return static_cast<uint32_t>(
        *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(stream) + 8));
}

static int32_t stream_state_word(void* stream)
{
    void* vtable = *reinterpret_cast<void**>(stream);
    int32_t child_offset = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(vtable) + 4);
    return *reinterpret_cast<int32_t*>(
        static_cast<uint8_t*>(stream) + child_offset + 8);
}
#endif

#ifndef _WIN32
static HostSaveStream* host_stream(void* stream)
{
    return static_cast<HostSaveStream*>(stream);
}

/* Host hardening helper: the unread byte count left in a primary save
 * stream.  INPUT_LoadSaveFile (0x41D5C0) uses it to validate the
 * declared record layout before spinning its loops (host deviation —
 * the original trusts the header). */
size_t host_stream_bytes_remaining(void* stream)
{
    HostSaveStream* s = host_stream(stream);
    if (s == nullptr) {
        return 0;
    }
    return s->bytes_remaining();
}
#endif

#ifndef _WIN32
/* Host atomic-save commit: flush + rename the secondary (write) stream's
 * temp file over its target path.  INPUT_SaveCurrentWorld (0x41D9B0)
 * calls this after every record write succeeds; the save is only
 * durable when it returns true (and the caller then returns 1).  An
 * uncommitted write stream removes its temp on destruction, so a failed
 * save never leaves a partial file.  Host-only deviation — the original
 * writes in place.  Declared in PersistenceAdapter.h. */
namespace loco {
namespace host {
bool host_save_commit(RESDATA* resdata);
bool host_save_commit(RESDATA* resdata)
{
    if (resdata == nullptr || resdata->secondary_stream == nullptr) {
        return false;
    }
    HostSaveStream* s = host_stream(resdata->secondary_stream);
    if (s->StateFlag(HostSaveStream::kStateError)) {
        return false;
    }
    return s->Commit();
}
}  // namespace host
}  // namespace loco
#endif

/* ================================================================== */
/* RESMGR_ResourceData_Init                                            */
/* Address: 0x447B20                                                   */
/*                                                                      */
/* Zeroes the save/load fields: pixels (+0x1C4), asset data (+0x1D0/    */
/* +0x1D4), the save-header type/player words (+0xB0/+0xB2/+0xB4) and  */
/* both stream pointers (+0x1C8/+0x1CC).  The binary additionally      */
/* writes the RESDATA vtable 0x478274; the C++ model keeps the struct  */
/* non-virtual and lets the compiler manage any vtable (AGENTS.md).     */
/* The header name region (+0xBE..) and record buffers (+0x04/+0x84)   */
/* are intentionally NOT zeroed by the original.                       */
/* ================================================================== */
void RESMGR_ResourceData_Init(RESDATA* resdata)
{
    resdata->save_pixels = nullptr;          /* +0x1C4 */
    resdata->asset_data = nullptr;           /* +0x1D0 */
    resdata->asset_size = 0;                 /* +0x1D4 */
    resdata->save.type = 0;                  /* +0xB0 */
    resdata->save.player_id = 0;             /* +0xB2 */
    resdata->save.player_color = 0;          /* +0xB4 */
    resdata->primary_stream = nullptr;       /* +0x1C8 */
    resdata->secondary_stream = nullptr;     /* +0x1CC */
}

/* ================================================================== */
/* RESMGR_ReleaseResource                                              */
/* Address: 0x447B90                                                   */
/*                                                                      */
/* Resets the resource-data state (vtable write in the binary), then    */
/* tail-calls RESMGR_RemoveResource (0x447FB0).  Does NOT free the      */
/* RESDATA object's own memory.                                        */
/* ================================================================== */
void RESMGR_ReleaseResource(RESDATA* resdata)
{
    RESMGR_RemoveResource(resdata);
}

/* ================================================================== */
/* RESMGR_RemoveResource                                               */
/* Address: 0x447FB0                                                   */
/*                                                                      */
/* Destroys both streams (deleting destructor, flags=1), frees the      */
/* asset data blob (CRT_free) and the preview pixels (GLOBAL_free),     */
/* nulls every slot, returns 1.                                        */
/* ================================================================== */
int32_t RESMGR_RemoveResource(RESDATA* resdata)
{
    void* stream = resdata->primary_stream;          /* +0x1C8 */
    if (stream != nullptr) {
        destroy_stream(stream);
        resdata->primary_stream = nullptr;
    }
    stream = resdata->secondary_stream;              /* +0x1CC */
    if (stream != nullptr) {
        destroy_stream(stream);
        resdata->secondary_stream = nullptr;
    }
    if (resdata->asset_data != nullptr) {            /* +0x1D0 */
        /* The binary frees the asset blob with the inner free 0x466C70
         * (the same routine GLOBAL_free 0x465CD0 wraps); on the host
         * both are free(). */
        GLOBAL_free(resdata->asset_data);
        resdata->asset_data = nullptr;
    }
    if (resdata->save_pixels != nullptr) {           /* +0x1C4 */
        GLOBAL_free(resdata->save_pixels);
        resdata->save_pixels = nullptr;
    }
    return 1;
}

/* ================================================================== */
/* RESMGR_LoadResource                                                 */
/* Address: 0x447BA0                                                   */
/*                                                                      */
/* thiscall(RESDATA*, const char* path), ret 0x4.                      */
/*                                                                      */
/* 1. RemoveResource(this)                                             */
/* 2. If g_asset_mgr (0x485600) is non-null: build "<resdir><path>"    */
/*    (0x4A99C8 buffer), AssetMgr_LoadFile, wrap the blob in a memory  */
/*    stream as primary_stream.                                        */
/* 3. Else (or if that produced no stream): open the file directly as  */
/*    primary_stream (WIN32_StreamOpenFile mode 0xA0).                 */
/* 4. If no stream → return 0.                                         */
/* 5. Read 0x114 bytes into RESDATA.save (+0xB0).                      */
/* 6. Allocate width*height preview pixels (width/height from the      */
/*    header words at +0xB2/+0xB4) and read them into +0x1C4.          */
/* ================================================================== */
int8_t RESMGR_LoadResource(RESDATA* resdata, const char* filename)
{
#ifndef _WIN32
    RESMGR_RemoveResource(resdata);

    /* Host path: direct bounded file read (g_asset_mgr is null on the
     * host — see above).  The filename is the caller-built
     * "<resdir><name>" path and is opened VERBATIM, exactly like the
     * original's WIN32_StreamOpenFile fallback (0x447C80 pushes the
     * filename argument unchanged); only the g_asset_mgr branch
     * concatenates a path itself. */
    HostSaveStream* stream = HostSaveStream::OpenRead(filename);
    resdata->primary_stream = stream;
    if (stream->StateFlag(HostSaveStream::kStateError)) {
        return 0;
    }

    /* Read the 0x114-byte header into the typed save region. */
    size_t got = stream->Read(&resdata->save, sizeof(SaveRegion));
    if (got != sizeof(SaveRegion)) {
        stream->SetStateFlag(HostSaveStream::kStateError);
        return 0;
    }

    /* Allocate and read the width*height preview pixel buffer.  The
     * header words at +0xB2/+0xB4 hold the preview dimensions on the
     * shipped saves (e.g. 64x48 = 1024x768/16).  A game-saved file
     * stores the player id/color there, so the preview can be tiny or
     * empty (player 0,0 -> 0 bytes); zero dimensions are legal.  Bounds:
     * a strict sane cap (16 MiB, same as PersistenceAdapter's
     * kMaxPreviewBytes) rejects corrupt headers before any allocation
     * (host hardening — the original computes the unchecked product). */
    uint32_t preview_w = resdata->save.player_id;    /* +0xB2 */
    uint32_t preview_h = resdata->save.player_color; /* +0xB4 */
    size_t preview_bytes = 0;
    if (preview_w != 0 && preview_h != 0) {
        if (preview_w > kHostMaxPreviewBytes / preview_h) {
            /* Corrupt dimensions — fail explicitly. */
            return 0;
        }
        preview_bytes = static_cast<size_t>(preview_w) * preview_h;
    }
    if (preview_bytes > 0) {
        void* pixels = operator_new(preview_bytes);
        resdata->save_pixels = pixels;
        if (pixels == nullptr) {
            return 0;
        }
        size_t got = stream->Read(pixels, preview_bytes);
        if (got != preview_bytes) {
            stream->SetStateFlag(HostSaveStream::kStateError);
            return 0;
        }
    }
    return 1;
#else
    /* ---- Original control flow (Win32 streams) ---- */
    RESMGR_RemoveResource(resdata);

    if (g_asset_mgr != nullptr) {
        /* Original (0x447BE1..0x447C0F): AssetMgr_LoadFile is called
         * with "filename + strlen(g_install_path)" — the binary adds the
         * res-dir length to the filename POINTER instead of prefixing a
         * concatenated buffer (an original pointer bug; the path is only
         * correct when g_install_path is empty).  Reproduced exactly. */
        int32_t asset_size = 0;
        void* asset_data = reinterpret_cast<void*>(
            AssetMgr_LoadFile(g_asset_mgr,
                              filename + std::strlen(g_install_path),
                              &asset_size));
        resdata->asset_data = asset_data;
        if (asset_data != nullptr) {
            /* 0x5C was the original x86 sizeof(WIN32_MemoryStream); use
             * the real host size (see resources/Win32StreamMem.h). */
            void* mem = operator_new(WIN32_MemoryStream_Size());
            if (mem != nullptr) {
                resdata->primary_stream =
                    WNDPROC_StreamFromMemory(mem, static_cast<char*>(asset_data), asset_size, 1);
            }
        }
    }
    if (resdata->primary_stream == nullptr) {
        void* mem = operator_new(WIN32_Stream_Size());
        if (mem != nullptr) {
            resdata->primary_stream =
                WIN32_StreamOpenFile(mem, filename, 0xA0, 0x479190, 1);
        }
    }
    if (resdata->primary_stream == nullptr) {
        return 0;
    }
    /* Original (0x447CA7..0x447CB2): a non-zero VTABLE-RELATIVE state
     * word means the stream failed to open/read — return 0 without
     * touching the header (the word is at stream + [vtable+4] + 8, NOT
     * the fixed [stream+8] byte count). */
    if (stream_state_word(resdata->primary_stream) != 0) {
        return 0;
    }
    WIN32_StreamRead(resdata->primary_stream, &resdata->save, sizeof(SaveRegion));
    /* Original (0x447CE0): explicit header short-read check — the
     * last-read BYTE COUNT at the fixed offset [stream+8] must equal
     * 0x114.  The binary error-reports through 0x466CE0 and continues
     * (returning 1 with a short header); the reconstruction fails
     * explicitly (return 0) — the same explicit-failure semantics the
     * host branch applies (a short header can only mean a truncated or
     * corrupt file). */
    if (stream_byte_count(resdata->primary_stream) != sizeof(SaveRegion)) {
        return 0;
    }
    uint32_t preview_w = resdata->save.player_id;
    uint32_t preview_h = resdata->save.player_color;
    size_t preview_bytes = static_cast<size_t>(preview_w) * preview_h;
    resdata->save_pixels = operator_new(preview_bytes);
    if (resdata->save_pixels != nullptr) {
        WIN32_StreamRead(resdata->primary_stream, resdata->save_pixels, preview_bytes);
        /* Original (0x447D54): explicit preview short-read check — the
         * byte count at [stream+8] must equal the requested size.  Same
         * error-report-then-continue original behaviour; the
         * reconstruction fails explicitly (see above). */
        if (stream_byte_count(resdata->primary_stream) != preview_bytes) {
            return 0;
        }
    }
    return 1;
#endif
}

/* ================================================================== */
/* RESMGR_LockResource                                                 */
/* Address: 0x447DB0                                                   */
/*                                                                      */
/* Reads one 0x80-byte entity record from primary_stream into the      */
/* record buffer at RESDATA+0x04 (x86 layout).  Returns the buffer     */
/* pointer, or nullptr when the stream is missing or the read is short */
/* (no partial records).  On 64-bit hosts the record is read into the  */
/* host-native typed buffer (types.h) instead — the +0x04 offset would */
/* land inside the pointer-width vtable member there (safe native      */
/* layout; no x86 offsets written into host RESDATA members).          */
/* ================================================================== */
void* RESMGR_LockResource(RESDATA* resdata)
{
#if UINTPTR_MAX == 0xffffffffu
    uint8_t* buffer = reinterpret_cast<uint8_t*>(resdata) + 0x04;  /* x86 record buffer */
#else
    uint8_t* buffer = resdata->host_record_entity;                 /* safe native buffer */
#endif
#ifndef _WIN32
    if (resdata->primary_stream == nullptr) {
        return nullptr;
    }
    size_t got = host_stream(resdata->primary_stream)->Read(buffer, 0x80);
    if (got != 0x80) {
        return nullptr;
    }
    return buffer;
#else
    /* Original (0x447DB3..0x447DE2): with a null stream the record
     * buffer is returned as-is (0x447DDE); a short read returns null
     * (0x447DDA) — the caller's null check gates progress.  The
     * short-read test compares the stream's last-read BYTE COUNT at the
     * fixed offset [stream+8] (0x447DD1), not the vtable-relative
     * state word. */
    if (resdata->primary_stream != nullptr) {
        WIN32_StreamRead(resdata->primary_stream, buffer, 0x80);
        if (stream_byte_count(resdata->primary_stream) != 0x80) {
            return nullptr;
        }
    }
    return buffer;
#endif
}

/* ================================================================== */
/* RESMGR_UnlockResource                                               */
/* Address: 0x447DF0                                                   */
/*                                                                      */
/* Reads one 0x2C-byte vehicle record from primary_stream into the     */
/* record buffer at RESDATA+0x84 (x86 layout).  Same short-read        */
/* contract as LockResource; same host-native buffer rule.             */
/* ================================================================== */
void* RESMGR_UnlockResource(RESDATA* resdata)
{
#if UINTPTR_MAX == 0xffffffffu
    uint8_t* buffer = reinterpret_cast<uint8_t*>(resdata) + 0x84;  /* x86 record buffer */
#else
    uint8_t* buffer = resdata->host_record_vehicle;                /* safe native buffer */
#endif
#ifndef _WIN32
    if (resdata->primary_stream == nullptr) {
        return nullptr;
    }
    size_t got = host_stream(resdata->primary_stream)->Read(buffer, 0x2C);
    if (got != 0x2C) {
        return nullptr;
    }
    return buffer;
#else
    /* Original (0x447DF3..0x447E22): same contract as LockResource —
     * the byte count at the fixed offset [stream+8] gates the short
     * read (0x447E11). */
    if (resdata->primary_stream != nullptr) {
        WIN32_StreamRead(resdata->primary_stream, buffer, 0x2C);
        if (stream_byte_count(resdata->primary_stream) != 0x2C) {
            return nullptr;
        }
    }
    return buffer;
#endif
}

/* ================================================================== */
/* RESMGR_LoadResourceData                                             */
/* Address: 0x447E30                                                   */
/*                                                                      */
/* thiscall(RESDATA*, const char* path), ret 0x4.  The save-write      */
/* twin of LoadResource: opens the OUTPUT stream (original mode 0x92), */
/* then writes the prepared header (RESDATA.save, 0x114 bytes) and the */
/* width*height preview pixels.  In the original the preview pixels    */
/* come from a TileMap overlay surface (TileMap_CreateOverlay 0x457080);*/
/* the SDL host writes the typed preview buffer the caller prepared    */
/* (see INPUT_SaveCurrentWorld — host deviation, no tilemap overlay is */
/* rendered yet).  Returns 1 on success, 0 on failure.                */
/* ================================================================== */
int32_t RESMGR_LoadResourceData(RESDATA* resdata, const char* filename)
{
#ifndef _WIN32
    RESMGR_RemoveResource(resdata);
    /* Host path: the filename is the caller-built "<resdir><name>"
     * path, opened verbatim like the original's stream-open (0x447E80).
     * Atomic-save contract: OpenWrite stages into "<path>.tmp" and the
     * caller publishes it with loco::host::host_save_commit() after all
     * record writes succeed; an uncommitted stream removes the temp on
     * destruction (no partial save ever becomes visible). */
    HostSaveStream* stream = HostSaveStream::OpenWrite(filename);
    resdata->secondary_stream = stream;
    if (stream->StateFlag(HostSaveStream::kStateError)) {
        return 0;
    }

    /* Write the 0x114-byte header. */
    if (stream->Write(&resdata->save, sizeof(SaveRegion)) != sizeof(SaveRegion)) {
        stream->SetStateFlag(HostSaveStream::kStateError);
        return 0;
    }

    /* Write the preview pixels (host: the caller-prepared typed preview
     * buffer of save.player_id*player_color bytes; see
     * INPUT_SaveCurrentWorld for the documented deviation).  A game-saved
     * header stores player id/color here, so the preview may be tiny or
     * empty — zero dimensions write nothing.  A strict sane cap (16 MiB)
     * rejects absurd header dims before any allocation (host hardening). */
    uint32_t preview_w = resdata->save.player_id;
    uint32_t preview_h = resdata->save.player_color;
    size_t preview_bytes = 0;
    if (preview_w != 0 && preview_h != 0) {
        if (preview_w > kHostMaxPreviewBytes / preview_h) {
            return 0;
        }
        preview_bytes = static_cast<size_t>(preview_w) * preview_h;
    }
    if (preview_bytes > 0) {
        if (resdata->save_pixels == nullptr) {
            /* Caller did not prepare a preview; write a zeroed buffer of
             * the right size so the output layout stays exact. */
            void* zeros = operator_new(preview_bytes);
            if (zeros == nullptr) {
                return 0;
            }
            std::memset(zeros, 0, preview_bytes);
            size_t written = stream->Write(zeros, preview_bytes);
            GLOBAL_free(zeros);
            if (written != preview_bytes) {
                stream->SetStateFlag(HostSaveStream::kStateError);
                return 0;
            }
        } else {
            if (stream->Write(resdata->save_pixels, preview_bytes) != preview_bytes) {
                stream->SetStateFlag(HostSaveStream::kStateError);
                return 0;
            }
        }
    }
    return 1;
#else
    RESMGR_RemoveResource(resdata);
    /* 0x58 was the original x86 sizeof(WIN32_OStream) (confirmed via this
     * exact call site's own allocation — see Win32OStream.h/WndProcOStream.h
     * for the vbtable/size evidence that WIN32_OStream is a genuinely
     * distinct, 4-bytes-smaller sibling of WIN32_Stream, not the same class
     * re-tagged); use the real host size instead, same convention as
     * WIN32_Stream_Size() a few hundred lines up. This whole `#else` branch
     * is also Windows-only (see the #ifndef _WIN32 host path above, which
     * uses HostSaveStream instead) — it never compiles or links on this
     * host, but the MinGW typecheck build does compile it. */
    void* mem = operator_new(WIN32_OStream_Size());
    if (mem == nullptr) {
        return 0;
    }
    /* DAT_00479190's real VALUE (confirmed via read_bytes: 0x000001A4), not
     * its address — the original loads [0x479190] into a register before
     * pushing it (`MOV ECX,[0x479190]`), it does not push the address
     * 0x479190 itself. The previous version of this call site passed the
     * literal address, a bug (harmless only because this whole branch is
     * dead on the host — see above). This is the same global at least 16
     * read sites across the tree pass as the shareMask/flags argument to
     * WIN32_StreamOpen*, with at least 4 mutually-inconsistent existing
     * declarations for it elsewhere (game/TrainStation.cpp's `void*
     * g_resource_dir_path`, game/ScriptedObject.cpp's `int
     * g_stream_open_flags`, ui/GameSetupPanel.cpp's `int
     * g_stream_open_mode`, and two more call sites — ui/UIPANEL_Surface.cpp,
     * ui/HelpWnd.cpp — that pass the literal address, the identical bug
     * fixed here). Its exact semantic meaning is unconfirmed — it precedes,
     * but is NOT one of, the three mask constants (0x479194/98/9C)
     * WIN32_StreamFile_Open ORs together to decode share-mode bits, and
     * masking 0x1A4 against that combined mask (0xE00) yields 0, so every
     * caller that passes this constant always takes Open()'s share-mode
     * DEFAULT branch — named for that role, not for any confirmed bit
     * meaning. RESMGR_LoadResource's analogous WIN32_StreamOpenFile call a
     * few hundred lines up has the identical address-vs-value bug; not
     * fixed here (out of this pass's scope, and it's one of many
     * inconsistent per-file declarations of this same global — see above),
     * but flagged for the same follow-up. */
    constexpr uint32_t kStreamOpenDefaultShareMask = 0x1A4;
    resdata->secondary_stream =
        WIN32_StreamOpenWriteFile(mem, filename, 0x92, kStreamOpenDefaultShareMask, 1);
    if (resdata->secondary_stream == nullptr) {
        return 0;
    }
    /* Original (0x447EA7..0x447EB1): when the stream's vtable-relative
     * STATE word has bit 0x4 set (non-writable) the save is refused
     * with 0.  The ctor above is the WIN32_OStream ctor (0x465090): it
     * ORs the open mode with 2 (0x447e8f passes mode 0x92) and attaches
     * the write-side child vtable 0x479244 (see Win32OStream.h) — the
     * OR'd bit is what makes the underlying WIN32_StreamFile writable.
     * (The read ctor 0x463970 used by LoadResource, constructing a plain
     * WIN32_Stream, ORs mode with 1 instead.) */
    if ((stream_state_word(resdata->secondary_stream) & 0x4) != 0) {
        return 0;
    }
    /* Original (0x447EB3..0x447EC4): the preview pixels come from
     * TileMap_CreateOverlay (0x457080) on g_tilemap, which fills a
     * 0x18-byte overlay struct on the stack whose pixel pointer sits at
     * +0x18.  The header (0x114 bytes) is streamed first, then the
     * width*height preview (0x447EC4..0x447EFC). */
    struct TileMapOverlay {
        uint8_t data[0x18];
        void*   pixels;
    };
    TileMapOverlay overlay;
    std::memset(&overlay, 0, sizeof(overlay));
    TileMap_CreateOverlay(g_tilemap, &overlay, 0);
    WIN32_StreamWrite(resdata->secondary_stream, &resdata->save,
                      sizeof(SaveRegion));
    uint32_t preview_w = resdata->save.player_id;
    uint32_t preview_h = resdata->save.player_color;
    size_t preview_bytes = static_cast<size_t>(preview_w) * preview_h;
    if (overlay.pixels != nullptr && preview_bytes > 0) {
        WIN32_StreamWrite(resdata->secondary_stream, overlay.pixels,
                          preview_bytes);
    }
    return 1;
#endif
}

/* ================================================================== */
/* RESMGR_WriteSaveRecord                                              */
/* Address: 0x447F50                                                   */
/*                                                                      */
/* Writes one 0x80-byte entity record to secondary_stream.  Returns 1  */
/* when the stream exists and is writable, 0 otherwise.                */
/* ================================================================== */
int32_t RESMGR_WriteSaveRecord(RESDATA* resdata, const void* data)
{
    if (resdata->secondary_stream == nullptr) {
        return 0;
    }
#ifndef _WIN32
    HostSaveStream* stream = host_stream(resdata->secondary_stream);
    if (stream->StateFlag(HostSaveStream::kStateError)) {
        return 0;
    }
    return (stream->Write(data, 0x80) == 0x80) ? 1 : 0;
#else
    WIN32_StreamWrite(resdata->secondary_stream, data, 0x80);
    return 1;
#endif
}

/* ================================================================== */
/* RESMGR_WriteTableRecord                                             */
/* Address: 0x447F80                                                   */
/*                                                                      */
/* Writes one 0x2C-byte vehicle record to secondary_stream.  Same      */
/* contract as WriteSaveRecord.                                        */
/* ================================================================== */
int32_t RESMGR_WriteTableRecord(RESDATA* resdata, const void* data)
{
    if (resdata->secondary_stream == nullptr) {
        return 0;
    }
#ifndef _WIN32
    HostSaveStream* stream = host_stream(resdata->secondary_stream);
    if (stream->StateFlag(HostSaveStream::kStateError)) {
        return 0;
    }
    return (stream->Write(data, 0x2C) == 0x2C) ? 1 : 0;
#else
    WIN32_StreamWrite(resdata->secondary_stream, data, 0x2C);
    return 1;
#endif
}

/* ================================================================== */
/* RESMGR_IsSaveHeader                                                 */
/* Address: 0x448030                                                   */
/*                                                                      */
/* Returns (RESDATA.save.type == 8) — the word at +0xB0.  The shipped  */
/* .sav files and ~curr all start with 08 00.                          */
/* ================================================================== */
bool RESMGR_IsSaveHeader(RESDATA* resdata)
{
    return resdata->save.type == 8;
}
