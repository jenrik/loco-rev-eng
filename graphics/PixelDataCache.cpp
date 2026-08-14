/**
 * PixelDataCache.cpp — PixelDataCache class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file implements the album pixel data cache (0x18-byte class,
 * vtable 0x4773E8). The cache stores PixelFormatEntry records
 * (each 0x18 bytes: 20-byte name + 4-byte value) loaded from
 * PostBag/AlbIndex_<player>_<idx>.ind files.
 *
 * Methods were originally Ghidra-named LOCOBITMAP_*:
 *   LOCOBITMAP_InsertPixelFormat -> Insert
 *   LOCOBITMAP_RemoveEntry -> RemoveEntry
 *   LOCOBITMAP_GetEntryCount -> GetEntryCount
 *   LOCOBITMAP_UnlockSurface -> Unlock
 *   LOCOBITMAP_LookupPixelFormat -> Lookup
 *   LOCOBITMAP_RemoveByAsset -> RemoveByAsset
 *   LOCOBITMAP_LookupAsset -> LookupAsset
 *   LOCOBITMAP_FlushPixelData -> Flush
 *   LOCOBITMAP_LoadPixelData -> Load
 */

// Status: TRANSCRIBED

#include "PixelDataCache.h"
#include "../game/PlayerConfig.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include <cstring>   /* memcpy, memset, strlen */
#include <cstdint>
#include <limits>

namespace {

/* +0x18 is PlayerConfig::player_id (game/PlayerConfig.h) — read directly
 * through the named field now that g_player_config is strongly typed
 * (2026-08-14; was a raw-offset uint8_t* read before that). */
static uint32_t read_player_number(const PlayerConfig* config)
{
    return static_cast<uint32_t>(config->player_id);
}

static uint32_t read_asset_value(const void* asset_desc)
{
    uint32_t value = 0;
    const auto* bytes = static_cast<const uint8_t*>(asset_desc);
    std::memcpy(&value, bytes + 0x0C, sizeof(value));
    return value;
}

static bool is_invalid_file_handle(const void* handle)
{
    return reinterpret_cast<uintptr_t>(handle) ==
           std::numeric_limits<uintptr_t>::max();
}

} // namespace

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* C++ allocation helpers */
void* operator_new(size_t size);                  /* @0x465CE0 */
void  GLOBAL_free(void* ptr);                     /* @0x465CD0 */

extern "C" {
    void  CRT_strupr(char* str);                  /* @0x474C70 -- ASCII in-place toupper */
    void  DDRAW_GetDdrawErrorString(int32_t err); /* @0x45BBC0 */

    /* File I/O */
    void* __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                                uint32_t dwShareMode, void* lpSecurityAttributes,
                                uint32_t dwCreationDisposition, uint32_t dwFlagsAndAttributes,
                                void* hTemplateFile);                     /* @IAT 0x4770B4 */
    bool  __stdcall ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                             uint32_t* lpNumberOfBytesRead, void* lpOverlapped); /* @IAT 0x4770BC */
    bool  __stdcall WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                              uint32_t* lpNumberOfBytesWritten, void* lpOverlapped); /* @IAT 0x4770A4 */
    bool  __stdcall CloseHandle(void* hObject);                           /* @IAT 0x4770A0 */
    uint32_t __stdcall GetFileSize(void* hFile, uint32_t* lpFileSizeHigh);/* @IAT 0x4770C0 */
    uint32_t __stdcall GetFileAttributesA(const char* lpFileName);       /* @IAT 0x47709C */
    bool  __stdcall DeleteFileA(const char* lpFileName);                  /* @IAT 0x4770B8 */
    uint32_t __stdcall GetLastError();                                    /* @IAT 0x4770B0 */
    uint32_t __stdcall FormatMessageA(uint32_t dwFlags, const void* lpSource,
                                      uint32_t dwMessageId, uint32_t dwLanguageId,
                                      char* lpBuffer, uint32_t nSize, void* Arguments); /* @IAT 0x4770AC */
    void*  __stdcall LocalFree(void* hMem);                               /* @IAT 0x4770A8 */

    /* CRT string formatting */
    int __cdecl wsprintfA(char* out, const char* format, ...);            /* @IAT 0x477370 */

    /* Network/asset lookup */
    bool  NET_CheckAssetExists(uint32_t asset_value, int32_t unknown, char* buffer); /* @0x445930 */
}
/* NET_ResolveAddress declared in network/DPlayManager.h (real C++ linkage,
 * DPlayManager* return, const char* param) — this extern "C" duplicate
 * removed 2026-08-14; it was a live landmine that made every real call
 * site always get null (see DPlayManager.h's own doc comment). */
#include "../network/DPlayManager.h"

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

/**
 * g_pixel_cache -- global PixelDataCache singleton pointer.
 * Address: 0x4FD3B4
 */
PixelDataCache* g_pixel_cache;   /* defined somewhere; actual storage at 0x4FD3B4 */

/* g_player_config now declared by network/DPlayManager.h (included below
 * for the real NET_ResolveAddress declaration) as the canonical
 * `PlayerConfig*` — this file's own weaker `uint8_t*` duplicate removed
 * 2026-08-14 (conflicted once both headers landed in the same TU). */

/**
 * g_install_path -- game install path string.
 * Address: 0x4A99C8
 *
 * Was declared here as `extern const char*` (a pointer) against the real
 * definition's `char[256]` (shared/stubs_impl.cpp) -- an array-vs-pointer
 * extern mismatch: reading "g_install_path" through the wrong declaration
 * loads the array's own first 8 bytes of string data as if they were a
 * stored pointer value, so any %s use of it dereferences garbage. Dormant
 * here only because this file's real PixelDataCache::Load/Flush were
 * never reachable before (the old code path called a no-op stub) --
 * confirmed live via a SIGSEGV repro (coredumpctl backtrace: strlen inside
 * vsnprintf's %s handling, `g_install_path` printing as an
 * unaccessible-memory pointer instead of a string).
 */
extern const char g_install_path[256];

/* Format strings for album index filename construction */
static const char kFormatPath[] = "%s%s%s_%03d_%04d.ind";  /* @0x47E0B0 */
static const char kPostBagDir[] = "PostBag";               /* @0x47E0C4 */
static const char kAlbIndexPrefix[] = "AlbIndex";          /* @0x47E0CC */

/* ================================================================== */
/* PixelDataCache::Create (constructor/factory)                        */
/* Address: 0x401620                                                   */
/*                                                                     */
/* Called by: GameLoop_Setup (0x406D1B)                                */
/* __fastcall (ECX = this pointer to pre-allocated 0x18-byte block)    */
/*                                                                     */
/* Sets vtable to 0x4773E8, initialises fields to sentinel values,     */
/* then loads album index 1 immediately.                               */
/* ================================================================== */
PixelDataCache* PixelDataCache::Create(void* mem)
{
    PixelDataCache* cache = static_cast<PixelDataCache*>(mem);
/* In the binary: cache->vtable = VTBL_*. Compiler-managed in natural C++. */
    cache->current_album_index = -1;                           /* +0x04 */
    cache->pixel_buffer        = nullptr;                      /* +0x08 */
    /* +0x0C buffer_size remains 0 from allocation zeroing */
    cache->insert_index        = -1;                           /* +0x10 */
    cache->saved_album_index   = -1;                           /* +0x14 */

    /* Load album index 1 (first letter category) on startup */
    cache->Load(1);

    return cache;
}

/* ================================================================== */
/* PixelDataCache::DestroyFromResource (vtable[0])                     */
/* Address: 0x401650 -- scalar deleting destructor                      */
/*                                                                     */
/* __thiscall (ECX = this), RET 0x4 (flags byte on stack).             */
/*                                                                     */
/* Flushes pixel data to disk, then optionally frees heap allocation.  */
/* ================================================================== */
void* PixelDataCache::DestroyFromResource(uint8_t flags)
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    this->Flush();
    return this;
}

/* ================================================================== */
/* PixelDataCache::Flush -- write pixel buffer to disk and reset        */
/* Address: 0x401C90                                                   */
/*                                                                     */
/* Called by: Load (0x401E0C), Dtor (0x401659), and via thunk          */
/*            at 0x401680 (from PlayerRecord / PlayerConfig_SetName)   */
/*                                                                     */
/* The thunk at 0x401680 is a 5-byte JMP 0x00401C90 -- a secondary     */
/* entry point for external callers (PlayerRecord_ctor,                 */
/* PlayerConfig_SetName) that invoke Flush via __fastcall.              */
/*                                                                     */
/* Builds path: <install>/PostBag/AlbIndex_<player>_<idx>.ind          */
/* Deletes existing file if present, then writes buffer contents and    */
/* frees the buffer. Resets current_album_index to -1 after flush.     */
/* ================================================================== */
void PixelDataCache::Flush()
{
    if (this->current_album_index == -1) {
        return;  /* nothing to flush */
    }

    /* Build output file path: <inst>/PostBag/AlbIndex_<pid>_<idx>.ind */
    char filepath[1284];
    uint32_t player_num = read_player_number(g_player_config);  /* player number from config */
    wsprintfA(filepath, kFormatPath,
              g_install_path, kPostBagDir, kAlbIndexPrefix,
              player_num, this->current_album_index);

    /* Delete any existing file */
    DeleteFileA(filepath);

    /* Only write if we have data */
    if ((this->buffer_size != 0) && (this->pixel_buffer != nullptr)) {
        /* Create the file (always create new, FILE_ATTRIBUTE_NORMAL) */
        void* hFile = CreateFileA(filepath,
                                  0xC0000000,    /* GENERIC_READ | GENERIC_WRITE */
                                  0x1,           /* FILE_SHARE_READ */
                                  nullptr,
                                  0x4,           /* OPEN_ALWAYS (actually CREATE_ALWAYS) */
                                  0x80,          /* FILE_ATTRIBUTE_NORMAL */
                                  nullptr);

        if (is_invalid_file_handle(hFile)) {
            /* File creation failed -- report error */
            uint32_t err = GetLastError();
            char* msg_buf = nullptr;
            FormatMessageA(0x1100,        /* FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                             FORMAT_MESSAGE_FROM_SYSTEM |
                                             FORMAT_MESSAGE_IGNORE_INSERTS */
                          nullptr, err,
                          0x400,          /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT) */
                          reinterpret_cast<char*>(&msg_buf), 0, nullptr);
            LocalFree(msg_buf);
            return;
        }

        /* Write the buffer contents */
        uint32_t bytes_written;
        BOOL write_ok = WriteFile(hFile, this->pixel_buffer,
                                  this->buffer_size, &bytes_written, nullptr);

        if (!write_ok) {
            /* Write failed -- report error */
            uint32_t err = GetLastError();
            char* msg_buf = nullptr;
            FormatMessageA(0x1100, nullptr, err,
                          0x400, reinterpret_cast<char*>(&msg_buf), 0, nullptr);
            LocalFree(msg_buf);
            return;  /* BUG: handle leaked (not closed on WriteFile failure) */
        }

        /* Close the file */
        CloseHandle(hFile);

        /* Free the buffer */
        GLOBAL_free(this->pixel_buffer);
        this->pixel_buffer  = nullptr;
        this->buffer_size   = 0;
    } else {
        /* No data to write -- just clear the buffer pointer */
        this->pixel_buffer  = nullptr;
    }

    this->current_album_index = -1;  /* reset album index */
    /* Note: +0x10 and +0x14 (insert_index, saved_album_index) are NOT cleared here */
}

/* ================================================================== */
/* PixelDataCache::Load -- load pixel data from album file              */
/* Address: 0x401DF0                                                   */
/*                                                                     */
/* __thiscall (ECX = this), RET 0x4 (1 stack arg = album_index).       */
/*                                                                     */
/* Loads the contents of AlbIndex_<player>_<idx>.ind into the buffer.  */
/* If the file doesn't exist (CreateFileA returns INVALID_HANDLE),     */
/* sets empty state (current_album_index = param, buffer=0, size=0).   */
/* If the file is empty or error reading, same empty-state result.     */
/* ================================================================== */
void PixelDataCache::Load(int32_t album_index)
{
    /* No-op if already on this album */
    if (album_index == this->current_album_index) {
        return;
    }

    /* Flush any currently loaded data */
    this->Flush();

    /* -1 means no album to load */
    if (album_index == -1) {
        return;
    }

    /* Build the file path */
    char filepath[1284];
    uint32_t player_num = read_player_number(g_player_config);
    wsprintfA(filepath, kFormatPath,
              g_install_path, kPostBagDir, kAlbIndexPrefix,
              player_num, album_index);

    /* Open the .ind file */
    void* hFile = CreateFileA(filepath,
                              0xC0000000,
                              0x1,
                              nullptr,
                              0x4,       /* OPEN_ALWAYS */
                              0x80,      /* FILE_ATTRIBUTE_NORMAL */
                              nullptr);

    if (is_invalid_file_handle(hFile)) {
        /* File doesn't exist -- set empty state */
        this->current_album_index = album_index;
        this->buffer_size = 0;
        this->pixel_buffer = nullptr;
        return;
    }

    /* Get the file size */
    uint32_t file_size = GetFileSize(hFile, nullptr);

    if (file_size == 0 || file_size == 0xFFFFFFFF) {
        /* Empty file or error -- set empty state */
        this->current_album_index = album_index;
        this->buffer_size = 0;
        this->pixel_buffer = nullptr;
        CloseHandle(hFile);
        return;
    }

    /* Allocate buffer and read file */
    if (file_size == 0) {
        this->pixel_buffer = nullptr;
        this->buffer_size = 0;
    } else {
        void* buffer = operator_new(file_size);
        this->pixel_buffer = static_cast<PixelFormatEntry*>(buffer);
        this->buffer_size = file_size;

        uint32_t bytes_read;
        BOOL read_ok = ReadFile(hFile, buffer, file_size, &bytes_read, nullptr);

        if (!read_ok) {
            /* Read failed -- report error */
            uint32_t err = GetLastError();
            char* msg_buf = nullptr;
            FormatMessageA(0x1100, nullptr, err,
                          0x400, reinterpret_cast<char*>(&msg_buf), 0, nullptr);
            LocalFree(msg_buf);
            /* BUG: buffer not freed on ReadFile failure; handle (hFile) also leaked */
            return;
        }
    }

    /* Update current album index */
    this->current_album_index = album_index;

    /* Close the file */
    CloseHandle(hFile);
}

/* ================================================================== */
/* PixelDataCache::GetEntryCount -- return entry count                  */
/* Address: 0x401810                                                   */
/*                                                                     */
/* __fastcall (ECX = this), result in EAX.                             */
/* Uses multiplication by 0xAAAAAAAB (unsigned division by 24 trick). */
/* ================================================================== */
int32_t PixelDataCache::GetEntryCount()
{
    return this->buffer_size / static_cast<int32_t>(sizeof(PixelFormatEntry));  /* / 0x18 */
}

/* ================================================================== */
/* PixelDataCache::Unlock -- switch album, return entry count           */
/* Address: 0x401820                                                   */
/*                                                                     */
/* __thiscall (ECX = this, 1 stack arg = album_index), RET 0x4.       */
/*                                                                     */
/* If the album_index differs from current, calls Load().              */
/* Returns the number of entries in the buffer.                        */
/* ================================================================== */
int32_t PixelDataCache::Unlock(int32_t album_index)
{
    if (this->current_album_index != album_index) {
        this->Load(album_index);
    }
    return this->buffer_size / static_cast<int32_t>(sizeof(PixelFormatEntry));
}

/* ================================================================== */
/* PixelDataCache::Insert -- insert a record at a sorted position       */
/* Address: 0x401690                                                   */
/*                                                                     */
/* __thiscall (ECX = this, 2 stack args: index + entry ptr), RET 0x8. */
/*                                                                     */
/* Reallocs buffer to make room for one 0x18-byte entry at the         */
/* given index. Copies data before/after the insertion point, inserts  */
/* the new entry, frees the old buffer.                                */
/* ================================================================== */
void PixelDataCache::Insert(int32_t index, const PixelFormatEntry* entry)
{
    /* Save current album index and set insertion index */
    this->saved_album_index = this->current_album_index;  /* +0x14 = +0x04 */
    this->insert_index      = index;                       /* +0x10 */

    int32_t old_size = this->buffer_size;
    int32_t new_size = old_size + static_cast<int32_t>(sizeof(PixelFormatEntry));

    /* Special case: first entry (buffer was empty) */
    if (this->pixel_buffer == nullptr) {
        PixelFormatEntry* new_buf =
            static_cast<PixelFormatEntry*>(operator_new(sizeof(PixelFormatEntry)));
        this->pixel_buffer = new_buf;
        this->buffer_size  = sizeof(PixelFormatEntry);
        *new_buf = *entry;  /* copy the 6-uint32 entry */
        return;
    }

    /* Allocate new larger buffer */
    PixelFormatEntry* new_buf =
        static_cast<PixelFormatEntry*>(operator_new(new_size));

    int32_t copy_before = index * static_cast<int32_t>(sizeof(PixelFormatEntry));
    if (copy_before > 0) {
        memcpy(new_buf, this->pixel_buffer, copy_before);
    }

    /* Copy the new entry into place */
    new_buf[index] = *entry;

    /* Copy remaining data after insertion point */
    int32_t remaining = old_size - copy_before;
    if (remaining > 0) {
        memcpy(&new_buf[index + 1],
               reinterpret_cast<PixelFormatEntry*>(
                   reinterpret_cast<uint8_t*>(this->pixel_buffer) + copy_before),
               remaining);
    }

    /* Free old buffer, assign new */
    GLOBAL_free(this->pixel_buffer);
    this->pixel_buffer = new_buf;
    this->buffer_size  = new_size;
}

/* ================================================================== */
/* PixelDataCache::RemoveEntry -- remove a record at index              */
/* Address: 0x401760                                                   */
/*                                                                     */
/* __thiscall (ECX = this, 1 stack arg = index), RET 0x4.              */
/*                                                                     */
/* If the buffer contains only 1 entry, frees the entire buffer and     */
/* resets to empty. Otherwise shrinks by 0x18 bytes, copying data      */
/* before and after the removal point, and freeing the old buffer.     */
/* ================================================================== */
void PixelDataCache::RemoveEntry(int32_t index)
{
    int32_t old_size = this->buffer_size;

    /* If only 1 entry remains, just free the whole buffer */
    if (old_size == static_cast<int32_t>(sizeof(PixelFormatEntry))) {
        GLOBAL_free(this->pixel_buffer);
        this->buffer_size  = 0;
        this->pixel_buffer = nullptr;
        return;
    }

    /* Allocate smaller buffer */
    int32_t new_size = old_size - static_cast<int32_t>(sizeof(PixelFormatEntry));
    PixelFormatEntry* new_buf =
        static_cast<PixelFormatEntry*>(operator_new(new_size));

    int32_t remove_offset = index * static_cast<int32_t>(sizeof(PixelFormatEntry));

    /* Only copy if the removal offset is within bounds */
    if (remove_offset < old_size) {
        /* Copy data before removal point */
        if (remove_offset > 0) {
            memcpy(new_buf, this->pixel_buffer, remove_offset);
        }

        /* Copy data after removal point */
        int32_t after_offset = (index + 1) * static_cast<int32_t>(sizeof(PixelFormatEntry));
        int32_t remaining = old_size - after_offset;
        if (remaining > 0) {
            memcpy(reinterpret_cast<uint8_t*>(new_buf) + remove_offset,
                   reinterpret_cast<uint8_t*>(this->pixel_buffer) + after_offset,
                   remaining);
        }

        /* Free old buffer, assign new */
        GLOBAL_free(this->pixel_buffer);
        this->pixel_buffer = new_buf;
        this->buffer_size  = new_size;
    }
}

/* ================================================================== */
/* PixelDataCache::Lookup -- find or insert entry by name               */
/* Address: 0x401850                                                   */
/*                                                                     */
/* __thiscall (ECX = this, 1 stack arg = asset_desc), RET 0x4.        */
/*                                                                     */
/* Categories by first letter, loads the category, linear-scans        */
/* for sorted insertion position, and always inserts (either finding   */
/* an existing entry position or inserting a new one).                  */
/*                                                                     */
/* Called by: NET_RegisterPlayer (0x444EF4)                             */
/* ================================================================== */
void PixelDataCache::Lookup(void* asset_desc)
{
    char name_copy[20];       /* local_18 -- copy of asset name (max 20 bytes) */
    char upper_name[20];      /* local_2c -- uppercase copy for categorization */
    uint32_t asset_value;     /* local_4 -- value from asset_desc+0x0c */

    const char* src_name = reinterpret_cast<const char*>(
        reinterpret_cast<uint8_t*>(asset_desc) + 0x25);  /* asset_desc+0x25 */

    /* Copy the asset name (with null terminator) */
    size_t name_len = strlen(src_name) + 1;
    if (name_len > sizeof(name_copy)) name_len = sizeof(name_copy);
    memcpy(name_copy, src_name, name_len);
    memset(name_copy + name_len, 0, sizeof(name_copy) - name_len);

    /* Also copy to upper_name buffer for uppercase conversion */
    memcpy(upper_name, src_name, name_len);
    memset(upper_name + name_len, 0, sizeof(upper_name) - name_len);

    /* Copy the asset value */
    asset_value = read_asset_value(asset_desc);

    /* Convert to uppercase for category determination */
    CRT_strupr(upper_name);

    /* Determine album category based on first letter of name */
    int32_t category;
    switch (upper_name[0]) {
    case 'A': case 'B': case 'C': category = 0; break;
    case 'D': case 'E': case 'F': category = 1; break;
    case 'G': case 'H': case 'I': case 'J': category = 2; break;
    case 'K': case 'L': case 'M': category = 3; break;
    case 'N': case 'O': case 'P': case 'Q': category = 4; break;
    case 'R': case 'S': case 'T': category = 5; break;
    case 'U': case 'V': case 'W': category = 6; break;
    case 'X': case 'Y': case 'Z': category = 7; break;
    default:                       category = 8; break;  /* non-alpha */
    }

    /* Load the category data */
    this->Load(category);

    int32_t entry_count = this->buffer_size / static_cast<int32_t>(sizeof(PixelFormatEntry));

    if (entry_count == 0) {
        /* Buffer is empty -- just insert at position 0 */
        PixelFormatEntry new_entry;
        memset(&new_entry, 0, sizeof(new_entry));
        memcpy(new_entry.name, name_copy, sizeof(new_entry.name));
        new_entry.value = asset_value;
        this->Insert(0, &new_entry);
        return;
    }

    /* Linear scan: find insertion position by case-sensitive name comparison */
    int32_t insert_pos = 0;
    uint8_t* buf_ptr = reinterpret_cast<uint8_t*>(this->pixel_buffer);

    /* Walk forward while name_copy > entry name (maintains sorted order) */
    while (insert_pos < entry_count) {
        PixelFormatEntry* entry =
            reinterpret_cast<PixelFormatEntry*>(buf_ptr + insert_pos * sizeof(PixelFormatEntry));
        int cmp = strcmp(name_copy, entry->name);
        if (cmp <= 0) {
            break;  /* found position (new name <= entry name) */
        }
        insert_pos++;
    }

    /* Check if an entry with this name already exists */
    if (insert_pos < entry_count) {
        PixelFormatEntry* check_entry =
            reinterpret_cast<PixelFormatEntry*>(buf_ptr + insert_pos * sizeof(PixelFormatEntry));
        if (strcmp(name_copy, check_entry->name) == 0) {
            /* Entry exists! Advance past any duplicates */
            while (insert_pos < entry_count) {
                check_entry = reinterpret_cast<PixelFormatEntry*>(
                    buf_ptr + insert_pos * sizeof(PixelFormatEntry));
                if (strcmp(name_copy, check_entry->name) != 0) {
                    break;
                }
                insert_pos++;
            }
        }
    }

    /* Insert the entry at the determined position */
    PixelFormatEntry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    memcpy(new_entry.name, name_copy, sizeof(new_entry.name));
    new_entry.value = asset_value;
    this->Insert(insert_pos, &new_entry);
}

/* ================================================================== */
/* PixelDataCache::RemoveByAsset -- remove entry by asset value         */
/* Address: 0x401AA0                                                   */
/*                                                                     */
/* __thiscall (ECX = this, 1 stack arg = asset_desc), RET 0x4.        */
/*                                                                     */
/* Returns TRUE (1) if match found and removed, FALSE (0) otherwise.   */
/* ================================================================== */
bool PixelDataCache::RemoveByAsset(void* asset_desc)
{
    char upper_name[20];
    const char* src_name = reinterpret_cast<const char*>(
        reinterpret_cast<uint8_t*>(asset_desc) + 0x25);

    /* Copy name and convert to uppercase for category determination */
    size_t name_len = strlen(src_name) + 1;
    if (name_len > sizeof(upper_name)) name_len = sizeof(upper_name);
    memcpy(upper_name, src_name, name_len);
    CRT_strupr(upper_name);

    /* Determine album category */
    int32_t category;
    switch (upper_name[0]) {
    case 'A': case 'B': case 'C': category = 0; break;
    case 'D': case 'E': case 'F': category = 1; break;
    case 'G': case 'H': case 'I': case 'J': category = 2; break;
    case 'K': case 'L': case 'M': category = 3; break;
    case 'N': case 'O': case 'P': case 'Q': category = 4; break;
    case 'R': case 'S': case 'T': category = 5; break;
    case 'U': case 'V': case 'W': category = 6; break;
    case 'X': case 'Y': case 'Z': category = 7; break;
    default:                       category = 8; break;
    }

    /* Load the category */
    this->Load(category);

    /* Check if buffer has data */
    if ((this->buffer_size == 0) || (this->pixel_buffer == nullptr)) {
        return false;
    }

    /* Copy the name again for comparison (yes, the original code copies it twice) */
    memcpy(upper_name, src_name, name_len);

    /* Get the value to match from asset_desc */
    uint32_t match_value = read_asset_value(asset_desc);

    /* Linear scan: find entry with matching value */
    int32_t entry_count = this->buffer_size / static_cast<int32_t>(sizeof(PixelFormatEntry));
    int32_t found_index = -1;

    for (int32_t i = 0; i < entry_count; i++) {
        if (this->pixel_buffer[i].value == match_value) {
            found_index = i;
            break;
        }
    }

    if (found_index != -1) {
        this->RemoveEntry(found_index);
        return true;
    }

    return false;
}

/* ================================================================== */
/* PixelDataCache::LookupAsset -- find valid asset from start index     */
/* Address: 0x401C10                                                   */
/*                                                                     */
/* __thiscall (ECX = this, 2 stack args: start_index + album_index),   */
/* RET 0x8.                                                            */
/*                                                                     */
/* Iterates entries from start_index, checking each via                 */
/* NET_CheckAssetExists/NET_ResolveAddress. Returns first valid        */
/* address or NULL.                                                    */
/* ================================================================== */
void* PixelDataCache::LookupAsset(int32_t start_index, int32_t album_index)
{
    this->Load(album_index);

    if (this->pixel_buffer == nullptr) {
        return nullptr;
    }

    /* Iterate from start_index */
    for (int32_t i = start_index; i < this->buffer_size / static_cast<int32_t>(sizeof(PixelFormatEntry)); i++) {
        char addr_buffer[1284];
        NET_CheckAssetExists(this->pixel_buffer[i].value, 0, addr_buffer);
        void* result = NET_ResolveAddress(addr_buffer);
        if (result != nullptr) {
            return result;
        }
    }

    return nullptr;
}
