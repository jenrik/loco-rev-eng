/**
 * win32_stream.c — Stream I/O wrapper functions (buffer, info, file, sync layers)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * ================================================================
 * Stream Architecture
 * ================================================================
 *
 * The stream system uses three layers:
 *
 * 1. StreamBuf layer (base buffer management):
 *    - WNDPROC_StreamBuf_Ctor/DtorBody  — buffer object lifecycle
 *    - WNDPROC_StreamBuf_SetBuffer      — assign buffer memory
 *    - WNDPROC_StreamBuf_CheckFlush     — flush unbuffered streams
 *
 * 2. StreamInfo layer (buffered stream with formatting):
 *    - WNDPROC_StreamCtor/Open/Close/Seek/Tell/Dtor/Flush/Printf
 *    - Operates on a larger block with anchored positions and
 *      custom allocator/free callbacks
 *    - WNDPROC_StreamVPrintf  — attach child stream
 *    - WNDPROC_StreamGetSize  — initialize info block
 *    - WNDPROC_StreamCleanup  — release info block
 *    - WNDPROC_StreamPutChar  — detach child stream
 *
 * 3. WIN32_StreamFile layer (OS file-backed):
 *    - WIN32_StreamFile_Ctor/DtorBody/CloseHandle/WriteChar/Flush/SetBuffer
 *    - Wraps CreateFile/ReadFile/WriteFile handles
 *
 * 4. Critical section wrappers:
 *    - WNDPROC_CriticalSectionInit/Unlock
 *    - WNDPROC_Initialize/Delete/Enter/LeaveCriticalSection
 *
 * === StreamInfo Block Layout ===
 * Offset   Size  Field
 * +0x00    4     vtable_ptr
 * +0x04    4     child_stream  — attached child (or 0)
 * +0x08    4     field_08      — error flags / type (init=4)
 * +0x0C    4     field_0C
 * +0x10    4     buffer_start
 * +0x14    4     buffer_end
 * +0x18    4     read_ptr
 * +0x1C    4     write_ptr
 * +0x20    4     high_water
 * +0x24    4     anchored_read
 * +0x28    4     anchored_write
 * +0x2C    4     anchored_end  (low byte = flags)
 * +0x30    4     field_30
 * +0x34    4     sync_active_1  — negative = cs1 active
 * +0x38    24    CRITICAL_SECTION cs1
 * +0x50    4     min_expand_size
 * +0x54    4     field_54
 * +0x58    4     field_58
 * +0x5C    4     alloc_callback  — NULL = operator_new
 * +0x60    4     free_callback   — NULL = GLOBAL_free
 * Total: 0x64 bytes
 *
 * === StreamBuf (base) Layout ===
 * Same fields 0x00-0x30, then:
 * +0x30    4     sync_active  — negative = CS active
 * +0x34    24    CRITICAL_SECTION
 * +0x4C    4     file_handle  (used by WIN32_StreamFile)
 * +0x50    4     field_50     (used by WIN32_StreamFile)
 * Total: 0x54 bytes
 *
 * === WIN32_StreamFile Vtable (0x479184) ===
 * [0] +0x00: scalar deleting destructor
 * [1] +0x04: close/write method
 * [2] +0x08: seek
 * [3] +0x0C: tell
 * [4] +0x10: flush
 * [5] +0x14: ???
 * [6] +0x18: read
 * [7] +0x1C: write?
 * [8] +0x20: getc
 * [9] +0x24: putc
 * [10] +0x28: flush for unbuffered
 */

#include <stdint.h>

/* ================================================================== */
/* Types                                                               */
/* ================================================================== */

/* Minimal CRITICAL_SECTION for Win32 (24 bytes on x86) */
typedef struct _CRITICAL_SECTION {
    void*      DebugInfo;
    int32_t    LockCount;
    int32_t    RecursionCount;
    void*      OwningThread;
    void*      LockSemaphore;
    uintptr_t  SpinCount;
} CRITICAL_SECTION;

/* ================================================================== */
/* External function declarations                                      */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);
extern void  __cdecl GLOBAL_free(void* ptr);

/* Win32 API through IAT */
extern void __cdecl WIN32_InitializeCriticalSection(CRITICAL_SECTION* lpcs);
extern void __cdecl WIN32_DeleteCriticalSection(CRITICAL_SECTION* lpcs);
extern void __cdecl WIN32_EnterCriticalSection(CRITICAL_SECTION* lpcs);
extern void __cdecl WIN32_LeaveCriticalSection(CRITICAL_SECTION* lpcs);

/* CRT functions */
extern int32_t __cdecl _isspace(int32_t c);
extern int32_t __cdecl _strtol(const char* str, char** endptr, int32_t radix);
extern int32_t* __cdecl _errno(void);

/* Stream internal read/parse functions (misnamed as CRT math in Ghidra) */
extern int32_t __fastcall StreamBuf_ReadChar(void* stream);       /* 0x4652A0 — misnamed CRT_tan */
extern int32_t __fastcall StreamBuf_GetChar(void* stream);        /* 0x4651A0 — misnamed CRT_log */
extern int32_t __fastcall StreamBuf_FlushOrPut(void* stream);     /* 0x465960 — misnamed CRT_ftolf */
extern int32_t __thiscall  StreamBuf_ReadString(void* _this, char* buf); /* 0x465AD0 — misnamed CRT_ftol */

/* WIN32_StreamFile constructor */
extern void* __fastcall WIN32_StreamFile_Ctor(void* mem);  /* 0x463B70 */

/* File open helper */
extern int32_t* __thiscall CRT_exp(void* file, const char* path, uint32_t mode, uint32_t flags);

/* ================================================================== */
/* Vtable / data symbols                                              */
/* ================================================================== */

extern void* PTR_WIN32_StreamFile_ScalarDtor_004791ac;  /* 0x4791AC — WIN32_StreamFile vtable */
extern void* PTR_LAB_00479184;                           /* 0x479184 — child stream vtable */
extern void* DAT_00479188;                               /* 0x479188 — stream vtable */
extern void* PTR_WIN32_StreamMem_ScalarDtor_004791dc;   /* 0x4791DC — mem stream vtable */
extern void* PTR_LAB_0047920c;                           /* 0x47920C — detached vtable */
extern void* PTR_LAB_00479234;                           /* 0x479234 — child-less vtable */
extern void* DAT_00479238;                               /* 0x479238 — vtable */
extern void* PTR_WNDPROC_StreamDtor_0047922c;            /* 0x47922C — StreamDtor vtable */
extern void* PTR_CRT_atan_00479254;                      /* 0x479254 — StreamBuf base vtable */

/* Global lock counter for stream subsystem */
extern int32_t         DAT_004ff180;     /* 0x4FF180 — stream object count */
extern CRITICAL_SECTION DAT_004ff148;    /* 0x4FF148 — global stream CS */


/* ================================================================== */
/* Stream Buffer Functions (StreamBuf base class)                      */
/* ================================================================== */

/**
 * WNDPROC_StreamBuf_Ctor — Base stream buffer constructor.
 * Address: 0x465470
 * Size: 66 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Zeros all fields from +0x04 through +0x2C, sets vtable to the base
 * StreamBuf vtable, sets field_0C and sync_active to -1, then
 * initializes the embedded CRITICAL_SECTION at +0x34.
 *
 * Called by:
 *   - WIN32_StreamFile_Ctor (0x463B73)
 *   - WIN32_StreamMem_Ctor (0x464010)
 *
 * @param _this  Pointer to uninitialized StreamBuf memory (0x4C+ bytes)
 */
void __fastcall WNDPROC_StreamBuf_Ctor(void* _this)
{
    int32_t* base = (int32_t*)_this;

    base[0x04 / 4] = 0;   /* +0x04 */
    base[0x08 / 4] = 0;   /* +0x08 */
    base[0x10 / 4] = 0;   /* +0x10 */
    base[0x14 / 4] = 0;   /* +0x14 */
    base[0x18 / 4] = 0;   /* +0x18 */
    base[0x1C / 4] = 0;   /* +0x1C */
    base[0x20 / 4] = 0;   /* +0x20 */
    base[0x24 / 4] = 0;   /* +0x24 */
    base[0x28 / 4] = 0;   /* +0x28 */
    base[0x2C / 4] = 0;   /* +0x2C */

    base[0x00 / 4] = (int32_t)&PTR_CRT_atan_00479254;  /* +0x00: vtable */
    base[0x0C / 4] = -1;                                /* +0x0C: field_0C = -1 */
    base[0x30 / 4] = -1;                                /* +0x30: sync_active = -1 */

    WNDPROC_InitializeCriticalSection(
        (CRITICAL_SECTION*)((uint8_t*)_this + 0x34));    /* +0x34: CRITICAL_SECTION */
}


/**
 * WNDPROC_StreamBuf_DtorBody — Base stream buffer destructor body.
 * Address: 0x4654E0
 * Size: 53 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Resets vtable to base StreamBuf vtable, deletes CRITICAL_SECTION,
 * then frees the buffer at +0x10 via GLOBAL_free if field_04 is
 * non-zero and buffer_start is non-NULL.
 *
 * Called by:
 *   - WNDPROC_StreamCtor (0x4640B0)
 *   - CRT_atan scalar destructor (0x4654C0)
 *
 * @param _this  Pointer to StreamBuf to tear down
 */
void __fastcall WNDPROC_StreamBuf_DtorBody(void* _this)
{
    int32_t* base = (int32_t*)_this;

    base[0x00 / 4] = (int32_t)&PTR_CRT_atan_00479254;  /* +0x00: reset vtable */

    WNDPROC_DeleteCriticalSection(
        (CRITICAL_SECTION*)((uint8_t*)_this + 0x34));    /* +0x34 */

    /* Free buffer if field_04 is non-zero and buffer_start is non-NULL */
    if ((base[0x04 / 4] != 0) && (*(void**)((uint8_t*)_this + 0x10) != NULL)) {
        GLOBAL_free(*(void**)((uint8_t*)_this + 0x10));  /* +0x10 */
    }
}


/**
 * WNDPROC_StreamBuf_SetBuffer — Sets the buffer pointers for a stream buffer.
 * Address: 0x465730
 * Size: 51 bytes
 * Calling convention: __thiscall (ECX = _this, 3 stack params)
 *
 * If a buffer is already present (field_04 != 0 and buffer_start != NULL),
 * frees it via GLOBAL_free first. Then assigns the new buffer pointers:
 * buffer_start = param_1, field_04 = param_3, buffer_end = param_2.
 *
 * Called by:
 *   - WNDPROC_StreamOpen (0x4641EC) — after reallocation
 *
 * @param _this      Stream buffer object
 * @param new_buf   New buffer base pointer
 * @param new_end   New buffer end pointer (one past last byte)
 * @param flags     Flags to store in field_04 (0 = read, non-zero = write)
 */
void __thiscall WNDPROC_StreamBuf_SetBuffer(
    void* _this, void* new_buf, void* new_end, int32_t flags)
{
    int32_t* base = (int32_t*)_this;

    /* Free old buffer if present */
    if ((base[0x04 / 4] != 0) && (*(void**)((uint8_t*)_this + 0x10) != NULL)) {
        GLOBAL_free(*(void**)((uint8_t*)_this + 0x10));  /* +0x10 */
    }

    /* Set new buffer pointers */
    *(void**)((uint8_t*)_this + 0x10) = new_buf;   /* +0x10: buffer_start */
    base[0x04 / 4] = flags;                        /* +0x04: field_04 */
    *(void**)((uint8_t*)_this + 0x14) = new_end;    /* +0x14: buffer_end */
}


/**
 * WNDPROC_StreamBuf_CheckFlush — Checks if stream buffer needs flushing.
 * Address: 0x4656D0
 * Size: 32 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * If both high_water (+0x20) and buffer_start (+0x10) are zero, the
 * buffer is "unbuffered" — calls the vtable[0x28] method to read/flush.
 * Returns 1 on success, 0 if buffer already has data, -1 on error.
 *
 * Called by: stream read-path functions when buffer is empty
 *
 * @param _this  StreamBuf pointer
 * @return      0 = buffer has data (no flush needed)
 *              1 = flushed successfully
 *             -1 = flush failed (EOF/error)
 */
int32_t __fastcall WNDPROC_StreamBuf_CheckFlush(void* _this)
{
    int32_t* base = (int32_t*)_this;

    /* If either high_water (+0x08) or buffer_start (+0x10) is non-zero,
     * buffer has data — no flush needed */
    if ((base[0x08 / 4] != 0) || (base[0x10 / 4] != 0)) {
        return 0;
    }

    /* Buffer is empty — call vtable[0x28] to read/flush */
    int32_t result = (**(int32_t (__fastcall**)(void))(
        *(void***)(base[0x00 / 4]))[0x28 / 4])();

    if (result == -1) {
        return -1;
    }
    return 1;
}


/* ================================================================== */
/* Stream Info Functions (WNDPROC layer)                              */
/* ================================================================== */

/**
 * WNDPROC_StreamCtor — Memory-backed stream destructor/cleanup.
 * Address: 0x4640B0
 * Size: 108 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * NOTE: Despite the name, _this is actually a DESTRUCTOR for a
 * memory-backed stream (WIN32_StreamMem). Sets vtable to the mem-stream
 * vtable, then in an SEH-protected block: if buffer (+0x10) is allocated
 * and field_4C is non-zero, frees via free_callback (+0x60) or GLOBAL_free.
 * Finally calls base WNDPROC_StreamBuf_DtorBody.
 *
 * Called by:
 *   - WIN32_StreamMem_ScalarDtor (0x463FD3)
 *
 * @param _this  Memory-backed stream to tear down
 */
void __fastcall WNDPROC_StreamCtor(void* _this)
{
    int32_t* base = (int32_t*)_this;

    /* SEH prologue (try block) */
    base[0x00 / 4] = (int32_t)&PTR_WIN32_StreamMem_ScalarDtor_004791dc;  /* +0x00: vtable */

    /* Free buffer if allocated and field_4C is set */
    if ((base[0x4C / 4] != 0) && (*(void**)((uint8_t*)_this + 0x10) != NULL)) {
        if (*(void**)((uint8_t*)_this + 0x60) == NULL) {  /* +0x60: free_callback */
            GLOBAL_free(*(void**)((uint8_t*)_this + 0x10));  /* +0x10: buffer_start */
        } else {
            (*(void (__cdecl**)(void))(base[0x60 / 4]))();  /* +0x60: custom free callback */
        }
    }
    /* SEH epilogue */

    WNDPROC_StreamBuf_DtorBody(_this);  /* call base destructor */
}


/**
 * WNDPROC_StreamOpen — Expand the stream buffer (reallocate larger).
 * Address: 0x464120
 * Size: 297 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Calculates a new buffer size based on min_expand_size (+0x50) and the
 * amount of data currently in the buffer (buffer_end - buffer_start).
 * Allocates new memory via alloc_callback (+0x5C) or operator_new.
 * Copies existing data from old buffer to new buffer.
 * Frees old buffer via free_callback (+0x60) or GLOBAL_free.
 * Updates all buffer pointers via WNDPROC_StreamBuf_SetBuffer.
 * Adjusts anchored positions and write position for the delta.
 *
 * NOTE: Despite the name "Open", _this is a buffer expansion / realloc
 * operation, not a file-open.
 *
 * Called by:
 *   - WNDPROC_StreamClose (0x46427D) — when write_pos hits high_water
 *
 * @param _this  StreamInfo block whose buffer needs expansion
 * @return      1 on success, -1 on failure (allocation failed)
 */
int32_t __fastcall WNDPROC_StreamOpen(void* _this)
{
    int32_t* base = (int32_t*)_this;
    int32_t new_size;
    int32_t used_bytes;
    void* new_buffer;
    int32_t delta;
    void* old_buf_start;

    int32_t min_expand = base[0x50 / 4];           /* +0x50: min_expand_size */
    int32_t cur_end    = base[0x14 / 4];           /* +0x14: buffer_end */
    int32_t cur_start  = base[0x10 / 4];           /* +0x10: buffer_start */
    int32_t alloc_size = (min_expand < 2) ? 1 : min_expand;

    /* Calculate used bytes */
    if (cur_end > cur_start) {
        used_bytes = cur_end - cur_start;
    } else {
        used_bytes = 0;
    }

    /* Determine total allocation size */
    if (min_expand <= used_bytes + alloc_size) {
        if (min_expand < 2) {
            min_expand = 1;
        }
        if (cur_end > cur_start) {
            used_bytes = cur_end - cur_start;
        } else {
            used_bytes = 0;
        }
        new_size = used_bytes + min_expand;
    } else {
        new_size = min_expand;
    }

    /* Allocate new buffer */
    if (base[0x5C / 4] == 0) {                    /* +0x5C: alloc_callback */
        new_buffer = operator_new(new_size);
    } else {
        new_buffer = (*(void* (__cdecl**)(int32_t))(base[0x5C / 4]))(new_size);
    }

    if (new_buffer == NULL) {
        return -1;
    }

    /* Copy existing data from old buffer to new buffer */
    old_buf_start = *(void**)((uint8_t*)_this + 0x10);  /* +0x10: buffer_start */
    if (cur_end > cur_start) {
        int32_t copy_size = cur_end - cur_start;
        void* dst = new_buffer;
        void* src = old_buf_start;
        int32_t i;

        /* Copy 4-byte chunks */
        for (i = copy_size >> 2; i != 0; i--) {
            *(int32_t*)dst = *(int32_t*)src;
            dst = (int32_t*)dst + 1;
            src = (int32_t*)src + 1;
        }
        /* Copy remaining bytes */
        for (i = copy_size & 3; i != 0; i--) {
            *(uint8_t*)dst = *(uint8_t*)src;
            dst = (uint8_t*)dst + 1;
            src = (uint8_t*)src + 1;
        }
        old_buf_start = *(void**)((uint8_t*)_this + 0x10);  /* +0x10 */
        delta = (int32_t)new_buffer - (int32_t)old_buf_start;
    } else {
        delta = 0;
    }

    /* Free old buffer */
    if (base[0x60 / 4] == 0) {                    /* +0x60: free_callback */
        GLOBAL_free(old_buf_start);
    } else {
        (*(void (__cdecl**)(void))(base[0x60 / 4]))();
    }

    /* Set new buffer via WNDPROC_StreamBuf_SetBuffer */
    WNDPROC_StreamBuf_SetBuffer(
        _this, new_buffer, (uint8_t*)new_buffer + new_size, 0);

    /* Adjust anchored positions by the delta */
    if ((delta != 0) && (base[0x2C / 4] != 0)) {  /* +0x2C: anchored_end */
        base[0x24 / 4] += delta;   /* +0x24: anchored_read */
        base[0x28 / 4] += delta;   /* +0x28: anchored_write */
        base[0x2C / 4] += delta;   /* +0x2C: anchored_end */
        base[0x0C / 4] = -1;       /* +0x0C: field_0C = -1 */
    }

    /* Adjust write position */
    if (base[0x20 / 4] != 0) {                    /* +0x20: high_water */
        int32_t old_write = base[0x1C / 4];        /* +0x1C: write_ptr */
        int32_t old_read  = base[0x18 / 4];        /* +0x18: read_ptr */
        int32_t new_hw    = base[0x20 / 4] + delta; /* +0x20 */
        int32_t new_read  = old_read + delta;

        base[0x20 / 4] = new_hw;       /* +0x20: high_water */
        base[0x18 / 4] = new_read;     /* +0x18: read_ptr */
        base[0x1C / 4] = new_read;     /* +0x1C: write_ptr reset to read_ptr */
        if (new_hw != 0) {
            base[0x1C / 4] = new_read + (old_write - old_read);  /* +0x1C: restore */
        }
    }

    return 1;
}


/**
 * WNDPROC_StreamClose — Write a single character to the stream buffer.
 * Address: 0x464260
 * Size: 138 bytes
 * Calling convention: __thiscall (ECX = _this, 1 stack param)
 *
 * NOTE: Despite the name, _this is a character-output operation (putc),
 * NOT a file-close operation. Writes param_1 to the current write_pos
 * if the buffer has space. If write_pos has reached high_water, calls
 * WNDPROC_StreamOpen to expand the buffer first. Returns 1 on success,
 * -1 on failure (buffer closed / expansion failed).
 *
 * Called by: stream write-path, usually via virtual dispatch
 *
 * @param _this  StreamInfo block
 * @param ch    Character byte to write (or -1 = no-op / skip)
 * @return      1 on success, -1 on failure
 */
int32_t __thiscall WNDPROC_StreamClose(void* _this, int32_t ch)
{
    int32_t* base = (int32_t*)_this;
    int32_t write_pos = base[0x1C / 4];  /* +0x1C: write_ptr */
    int32_t hw        = base[0x20 / 4];  /* +0x20: high_water */

    /* Check if write_pos has reached high_water — need to expand */
    if (write_pos >= hw) {
        if (base[0x4C / 4] == 0) {       /* +0x4C: closed flag (0 = closed) */
            return -1;
        }

        if (WNDPROC_StreamOpen(_this) == -1) {
            return -1;
        }

        /* Re-read positions after expansion */
        hw = base[0x20 / 4];             /* +0x20: high_water */

        if (hw == 0) {
            /* First-time buffer setup: compute initial positions */
            int32_t end         = base[0x14 / 4];  /* +0x14: buffer_end */
            int32_t start       = base[0x10 / 4];  /* +0x10: buffer_start */
            int32_t anchor_end  = base[0x2C / 4];  /* +0x2C: anchored_end */
            int32_t anchor_read = base[0x24 / 4];  /* +0x24: anchored_read */
            int32_t init_pos    = (anchor_end - anchor_read) + start;

            base[0x18 / 4] = init_pos;              /* +0x18: read_ptr */
            base[0x1C / 4] = init_pos;              /* +0x1C: write_ptr */
            base[0x20 / 4] = base[0x14 / 4];        /* +0x20: high_water = buffer_end */
        } else {
            int32_t new_read  = base[0x18 / 4];     /* +0x18: read_ptr */
            int32_t old_write = base[0x1C / 4];     /* +0x1C: write_ptr */

            base[0x1C / 4] = new_read;              /* +0x1C: reset write = read */
            base[0x20 / 4] = base[0x14 / 4];        /* +0x20: high_water = buffer_end */
            if (base[0x14 / 4] != 0) {              /* +0x14: buffer_end */
                base[0x1C / 4] = new_read + (old_write - new_read);  /* restore */
            }
        }
    }

    /* Write the character if ch != -1 */
    if (ch != -1) {
        *(uint8_t*)(base[0x1C / 4]) = (uint8_t)ch;  /* +0x1C: write character */
        if (base[0x20 / 4] != 0) {                 /* +0x20: high_water */
            base[0x1C / 4] += 1;                    /* +0x1C: advance write_ptr */
        }
    }

    return 1;
}


/**
 * WNDPROC_StreamSeek — Reset stream connector to detached state.
 * Address: 0x464550
 * Size: 19 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Extracts the child stream area offset from vtable[1], writes the
 * detached vtable (0x47920C) there, then calls WNDPROC_StreamPutChar
 * to detach any attached child. Effectively seeks/resets the stream
 * chain for cleanup.
 *
 * Called by: cleanup/detach paths
 *
 * @param _this  StreamInfo block
 */
void __fastcall WNDPROC_StreamSeek(void* _this)
{
    int32_t* vtable = *(int32_t**)_this;     /* +0x00 */
    int32_t  offset = vtable[0x04 / 4];     /* vtable[1] = child area offset */

    /* Write detached vtable to child area */
    *(void**)((uint8_t*)_this + offset) = &PTR_LAB_0047920c;

    /* Detach child stream */
    WNDPROC_StreamPutChar(_this);
}


/**
 * WNDPROC_StreamTell — Report stream size and release resources.
 * Address: 0x464570
 * Size: 20 bytes
 * Calling convention: __thiscall (ECX = _this)
 *
 * Calls WNDPROC_StreamGetSize on (_this + 8) to populate the size info,
 * then calls WNDPROC_StreamCleanup to release resources.
 *
 * NOTE: Despite the name, _this is more of a "cleanup and report size"
 * operation, not a simple tell-position query.
 *
 * Called by: stream close/cleanup paths
 *
 * @param _this  StreamInfo block
 */
void __thiscall WNDPROC_StreamTell(void* _this)
{
    void* info_block = (uint8_t*)_this + 8;

    WNDPROC_StreamGetSize(info_block);  /* populate size info at +0x08 */
    WNDPROC_StreamCleanup(info_block);  /* release resources */
}


/**
 * WNDPROC_StreamDtor — Scalar deleting destructor for stream info block.
 * Address: 0x464600
 * Size: 30 bytes
 * Calling convention: __thiscall (ECX = _this, 1 stack param)
 *
 * Calls WNDPROC_StreamCleanup to release resources, then optionally
 * frees the object memory via GLOBAL_free if (flags & 1) != 0.
 * Returns the _this pointer.
 *
 * This is the vtable[0] entry for stream info blocks.
 *
 * Called by: client code via vtable dispatch
 *
 * @param _this   Stream info block to destroy
 * @param flags  Bit 0 = free memory (scalar deleting destructor flag)
 * @return       The original _this pointer
 */
void* __thiscall WNDPROC_StreamDtor(void* _this, uint8_t flags)
{
    WNDPROC_StreamCleanup(_this);
}


/**
 * WNDPROC_StreamFlush — Flush data through the stream chain.
 * Address: 0x464680
 * Size: 55 bytes
 * Calling convention: __thiscall (ECX = _this, 1 stack param)
 *
 * If child_stream is attached and write_ptr is non-zero, calls the
 * child's vtable[0] (first method — usually a flush/close). Then stores
 * param_1 as the new child_stream field. Updates error flags:
 *   - param_1 != 0: clears bit 2 (release/OK)
 *   - param_1 == 0: sets bit 2 (close/EOF marker)
 *
 * Called by:
 *   - WNDPROC_StreamVPrintf (0x464840) — attaching new child
 *   - External code via vtable dispatch
 *
 * @param _this       StreamInfo block
 * @param new_child  New child stream pointer (or NULL = detach/error)
 */
void __thiscall WNDPROC_StreamFlush(void* _this, int32_t new_child)
{
    int32_t* base = (int32_t*)_this;

    /* If write_ptr is non-zero and child_stream exists, flush child */
    if ((base[0x1C / 4] != 0) && (base[0x04 / 4] != 0)) {  /* +0x1C, +0x04 */
        void* child = *(void**)((uint8_t*)_this + 0x04);
        (*(void (__thiscall**)(void*, int32_t))child)(child, 1);  /* child->vtable[0](1) */
    }

    /* Store new child value */
    base[0x04 / 4] = new_child;   /* +0x04: child_stream */

    /* Update error flags based on new_child */
    if (new_child != 0) {
        base[0x08 / 4] &= ~4;     /* +0x08: clear bit 2 */
    } else {
        base[0x08 / 4] |= 4;      /* +0x08: set bit 2 (closed/EOF) */
    }
}


/**
 * WNDPROC_StreamPrintf — Read a numeric token from stream, convert to uint16.
 * Address: 0x464750
 * Size: 183 bytes
 * Calling convention: __thiscall (ECX = _this, 1 stack param)
 *
 * NOTE: This is NOT a printf-style formatting function. The name is
 * misleading — it actually READS from the stream. Steps:
 *
 * 1. Acquires critical sections via WNDPROC_CriticalSectionInit
 * 2. Calls StreamBuf_ReadString (misnamed CRT_ftol) to read a text
 *    token into a local 16-byte buffer
 * 3. Calls _strtol to parse the token as an integer
 * 4. Validates the result fits in uint16 range:
 *    - If 0 <= value <= 0xFFFF or 0xFFFF7FFF < value <= 0xFFFFFFFF
 *      (signed negative wraparound), writes value as uint16 to *output
 *    - Otherwise writes 0xFFFF and sets error flag bit 1
 * 5. Releases all critical sections
 *
 * Called by: UI_ChildWindow_Render, INPUT_EditWndProc, and many
 *            serialization paths — used to parse uint16 fields from
 *            the stream during save file loading or format parsing.
 *
 * Range check detail (value is signed int32_t):
 *   - Direct pass: (value >= 0 && value <= 0xFFFF) — normal 0..65535
 *   - Negative wraparound: (value > 0xFFFF7FFF) — catches values
 *     like 0xFFFF8000...0xFFFFFFFF which are -32768..-1 as signed;
 *     the unsigned representation would be >16-bit, but loading as
 *     uint16 gives the intended negative-int16 interpretation
 *   - EILSEQ: (value == -1 && errno == 0x22) — strtol no conversion
 *
 * @param _this    StreamInfo block
 * @param output  Output pointer for the parsed uint16 value
 * @return        The _this pointer
 */
void* __thiscall WNDPROC_StreamPrintf(void* _this, uint16_t* output)
{
    int32_t* base = (int32_t*)_this;
    char token_buf[16];
    int32_t radix;
    int32_t value;
    int32_t child_offset;

    /* Acquire critical sections */
    if (WNDPROC_CriticalSectionInit(_this, 0) == 0) {
        return _this;
    }

    /* Read token string from stream */
    radix = StreamBuf_ReadString(_this, token_buf);  /* CRT_ftol — reads token, returns radix */

    /* Convert token string to integer via strtol */
    value = _strtol(token_buf, NULL, radix);  /* _srand_wrapper */

    /* Validate range for uint16 output
     * Three passing conditions:
     * 1. Normal: 0 <= value <= 0xFFFF
     * 2. Signed-int16 wraparound: 0xFFFF7FFF < value (e.g. -1 as signed = 0xFFFFFFFF)
     * 3. No conversion: value == -1 && errno == EILSEQ (0x22) — treat as valid 0xFFFF
     */
    if ((value >= 0 && value <= 0xFFFF) ||
        (value > 0xFFFF7FFF) ||
        (value == -1 && *_errno() == 0x22))
    {
        *output = (uint16_t)value;
    } else {
        /* Overflow — clamp and set error */
        *output = 0xFFFF;
        child_offset = *(int32_t*)(base[0x00 / 4] + 4);   /* vtable[1] = child offset */
        *(uint32_t*)((uint8_t*)_this + child_offset + 8) |= 2;  /* +0x08: set error bit 1 */
    }

    /* Release critical sections */

    /* 1. Child stream read CS (at child_stream->0x34) */
    child_offset = *(int32_t*)(base[0x00 / 4] + 4);       /* vtable[1] */
    {
        int32_t* child_stream = *(int32_t**)((uint8_t*)_this + child_offset + 4);
        if (child_stream != NULL && child_stream[0x30 / 4] < 0) {  /* +0x30: sync_active */
            WNDPROC_LeaveCriticalSection(
                (CRITICAL_SECTION*)((uint8_t*)child_stream + 0x34));  /* +0x34: CS */
        }
    }

    /* 2. StreamInfo write CS (at child_area + 0x38) */
    {
        int32_t sync_val = *(int32_t*)((uint8_t*)_this + child_offset + 0x34);  /* +0x34 */
        if (sync_val < 0) {
            WNDPROC_LeaveCriticalSection(
                (CRITICAL_SECTION*)((uint8_t*)_this + child_offset + 0x38));  /* +0x38: CS */
        }
    }

    return _this;
}


/**
 * WNDPROC_StreamVPrintf — Attach a child stream to the parent connector.
 * Address: 0x464840
 * Size: ~200 bytes
 * Calling convention: __thiscall (ECX = _this, 2 stack params)
 *
 * If flags is non-zero, initializes the info block via
 * WNDPROC_StreamGetSize. Detaches any existing child by writing the
 * child-less vtable, attaches the new child via WNDPROC_StreamFlush,
 * sets write-flag (bit 0 at anchored_end low byte), and resets
 * position tracking fields.
 *
 * Called by:
 *   - WIN32_StreamOpen (0x46390C) — attach WIN32_StreamFile child
 *   - WIN32_StreamOpenFile (0x463A4F) — same
 *
 * @param _this   Top-level stream object
 * @param child  Child stream to attach (or NULL)
 * @param flags  Non-zero = also init info block at +0x0C
 * @return       The _this pointer
 */
void* __thiscall WNDPROC_StreamVPrintf(void* _this, void* child, int32_t flags)
{
    int32_t* base = (int32_t*)_this;
    int32_t  child_offset;

    if (flags != 0) {
        /* SEH prologue */
        *(void**)_this = &DAT_00479238;                 /* +0x00: vtable */
        WNDPROC_StreamGetSize((void*)((uint8_t*)_this + 0x0C));  /* init info at +0x0C */
    }

    /* Get the child stream area offset from vtable[1] */
    child_offset = *(int32_t*)(base[0x00 / 4] + 4);   /* vtable[1] */

    /* Detach any existing child by setting child-less vtable */
    *(void**)((uint8_t*)_this + child_offset) = &PTR_LAB_00479234;

    /* Attach new child via flush */
    WNDPROC_StreamFlush((void*)((uint8_t*)_this + child_offset), (int32_t)child);

    /* Set write-flag (bit 0 of anchored_end low byte) */
    *(uint32_t*)((uint8_t*)_this + child_offset + 0x2C) |= 1;   /* +0x2C: flag bit 0 */

    /* Reset position tracking */
    *(int32_t*)((uint8_t*)_this + 8) = 0;   /* +0x08: bytes_read = 0 */
    *(int32_t*)((uint8_t*)_this + 4) = 0;   /* +0x04: child ptr override = 0 */

    /* SEH epilogue */
    return _this;
}


/**
 * WNDPROC_StreamGetSize — Initialize a StreamInfo size/state block.
 * Address: 0x464590
 * Size: ~50 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Sets up a new StreamInfo block with default values:
 * vtable = StreamDtor vtable, type = 4, mode = 6, flags = 0x20,
 * sync_active = -1. Initializes the embedded CRITICAL_SECTION.
 * Increments global stream object counter; if first object, also
 * initializes the global stream CRITICAL_SECTION.
 *
 * Called by:
 *   - WIN32_StreamOpen (0x4638C8)
 *   - WNDPROC_StreamVPrintf (internally when flags != 0)
 *   - WNDPROC_StreamTell (0x464576)
 *
 * @param _this  Memory for StreamInfo block (0x64+ bytes)
 */
void __fastcall WNDPROC_StreamGetSize(void* _this)
{
    int32_t* base = (int32_t*)_this;

    base[0x04 / 4] = 0;       /* +0x04: child_stream = 0 */
    base[0x0C / 4] = 0;       /* +0x0C: field_0C = 0 */
    base[0x10 / 4] = 0;       /* +0x10: buffer_start = 0 */
    base[0x20 / 4] = 0;       /* +0x20: high_water = 0 */
    base[0x24 / 4] = 0;       /* +0x24: anchored_read = 0 */
    base[0x2C / 4] = 0;       /* +0x2C: anchored_end = 0 */
    base[0x1C / 4] = 0;       /* +0x1C: write_ptr = 0 */

    base[0x00 / 4] = (int32_t)&PTR_WNDPROC_StreamDtor_0047922c;  /* +0x00: vtable */
    base[0x08 / 4] = 4;       /* +0x08: type = 4 */
    base[0x28 / 4] = 6;       /* +0x28: mode = 6 */
    *(uint8_t*)((uint8_t*)_this + 0x2F) = 0x20;     /* +0x2C low byte = flags 0x20 */

    base[0x34 / 4] = -1;      /* +0x34: sync_active = -1 */
    WNDPROC_InitializeCriticalSection(
        (CRITICAL_SECTION*)((uint8_t*)_this + 0x38));  /* +0x38: CS */

    /* Increment global stream counter; if first one, init global CS */
    if (DAT_004ff180 == 0) {
        WNDPROC_InitializeCriticalSection(&DAT_004ff148);
    }
    DAT_004ff180++;
}


/**
 * WNDPROC_StreamCleanup — Release stream info block resources.
 * Address: 0x464620
 * Size: ~100 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Resets vtable to destructor vtable (0x47922C), sets sync_active to -1,
 * decrements global stream counter, deletes its CS, and if child stream
 * is attached, calls child->vtable[0](1) to destroy it. Resets child
 * pointer and type field.
 *
 * Called by:
 *   - WNDPROC_StreamDtor (0x464603)
 *   - WNDPROC_StreamTell (0x46457D)
 *
 * @param _this  StreamInfo block to clean up
 */
void __fastcall WNDPROC_StreamCleanup(void* _this)
{
    int32_t* base = (int32_t*)_this;

    base[0x00 / 4] = (int32_t)&PTR_WNDPROC_StreamDtor_0047922c;  /* +0x00: vtable */
    base[0x34 / 4] = -1;       /* +0x34: sync_active = -1 */

    /* Decrement global counter; if last one, delete global CS */
    DAT_004ff180--;
    if (DAT_004ff180 == 0) {
        WNDPROC_DeleteCriticalSection(&DAT_004ff148);
    }

    /* Delete our CS */
    WNDPROC_DeleteCriticalSection(
        (CRITICAL_SECTION*)((uint8_t*)_this + 0x38));  /* +0x38 */

    /* Destroy attached child stream if present */
    if ((base[0x1C / 4] != 0) && (*(void**)((uint8_t*)_this + 0x04) != NULL)) {
        void* child = *(void**)((uint8_t*)_this + 0x04);  /* +0x04 */
        (*(void (__thiscall**)(void*, int32_t))child)(child, 1);  /* child->vtable[0](1) */
    }

    base[0x04 / 4] = 0;   /* +0x04: clear child ptr */
    base[0x08 / 4] = 4;   /* +0x08: reset type to 4 */
}


/**
 * WNDPROC_StreamPutChar — Detach the child stream from the connector.
 * Address: 0x4648E0
 * Size: ~15 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Writes the child-less vtable pointer (0x479234) to the child-stream
 * field, effectively detaching any attached child. The child stream
 * area offset is extracted from vtable[1].
 *
 * Called by:
 *   - WNDPROC_StreamSeek (0x46455E) — during stream seek/reset
 *   - WIN32_StreamClose (0x463A60) — during top-level stream close
 *
 * @param _this  Parent connector (StreamInfo block)
 */
void __fastcall WNDPROC_StreamPutChar(void* _this)
{
    int32_t* vtable = *(int32_t**)_this;     /* +0x00 */
    int32_t  offset = vtable[0x04 / 4];     /* vtable[1] = child area offset */

    *(void**)((uint8_t*)_this + offset) = &PTR_LAB_00479234;
}


/* ================================================================== */
/* Stream Synchronization (Critical Section Management)                */
/* ================================================================== */

/**
 * WNDPROC_CriticalSectionInit — Acquire stream critical sections for I/O.
 * Address: 0x4648F0
 * Size: 251 bytes
 * Calling convention: __thiscall (ECX = _this, 1 stack param)
 *
 * Acquires the stream's write CRITICAL_SECTION (at child_offset + 0x38).
 * If mode != 0, resets the bytes_read field. Checks error flags; if
 * error bit 0 is set (already-closed), sets bit 1 and returns 0.
 *
 * If the buffer is near full (high_water reached with data pending),
 * calls StreamBuf_FlushOrPut (misnamed CRT_ftolf) to push data through.
 *
 * Then acquires the read CRITICAL_SECTION (in child stream at +0x34).
 * If mode == 0 and write-flag bit 0 is set, calls
 * WNDPROC_CriticalSectionUnlock to skip whitespace in the stream.
 *
 * Returns 1 on success (ready for I/O), 0 on error/closed.
 *
 * Called by:
 *   - WNDPROC_StreamPrintf (0x464759) — mode = 0 (read mode)
 *   - WIN32_StreamRead (0x463810)     — mode = 1 (write mode)
 *   - StreamBuf_ReadString (0x465AD0) — internal parse path
 *   - StreamBuf_FlushOrPut (0x465960) — internal flush path
 *
 * @param _this  StreamInfo block
 * @param mode  0 = read mode (skip whitespace), non-zero = write mode
 * @return      1 = ready, 0 = error/closed
 */
int32_t __thiscall WNDPROC_CriticalSectionInit(void* _this, int32_t mode)
{
    int32_t* base = (int32_t*)_this;
    int32_t  child_offset;
    int32_t* child_area;
    int32_t  error_flags;

    /* Get child stream area offset from vtable[1] */
    child_offset = *(int32_t*)(base[0x00 / 4] + 4);   /* vtable[1] */
    child_area = (int32_t*)((uint8_t*)_this + child_offset);

    /* Acquire write CS (at child_area + 0x38) */
    if (child_area[0x34 / 4] < 0) {                   /* +0x34: sync_active */
        WNDPROC_EnterCriticalSection(
            (CRITICAL_SECTION*)((uint8_t*)child_area + 0x38));  /* +0x38: CS */
    }

    /* Reset bytes_read in write mode */
    if (mode != 0) {
        *(int32_t*)((uint8_t*)_this + 8) = 0;   /* +0x08: bytes_read = 0 */
    }

    /* Check error flags — if bit 0 is set, stream is closed */
    error_flags = child_area[0x08 / 4];              /* +0x08: error flags */
    if (error_flags != 0) {
        child_area[0x08 / 4] = error_flags | 2;      /* set bit 1 (already-closed) */
        goto error_exit;
    }

    /* Flush buffer if high_water is full with pending data */
    {
        int32_t* child_stream = *(int32_t**)((uint8_t*)child_area + 4);  /* child_area->child */
        if (child_stream != NULL) {
            if (mode != 0) {
                int32_t anchored_end   = child_stream[0x2C / 4];  /* +0x2C */
                int32_t anchored_write = child_stream[0x28 / 4];  /* +0x28 */
                int32_t pending;

                if (anchored_write < anchored_end) {
                    pending = anchored_end - anchored_write;
                } else {
                    pending = 0;
                }
                if (mode <= pending) {
                    goto skip_flush;
                }
            }
            StreamBuf_FlushOrPut(child_stream);  /* CRT_ftolf — push data through */
        }
    }
skip_flush:

    /* Acquire read CS (in child stream at +0x34) */
    {
        int32_t* child_stream = *(int32_t**)((uint8_t*)child_area + 4);
        if (child_stream != NULL && child_stream[0x30 / 4] < 0) {  /* child->sync_active */
            WNDPROC_EnterCriticalSection(
                (CRITICAL_SECTION*)((uint8_t*)child_stream + 0x34));  /* child->CS */
        }
    }

    /* In read mode, if write-flag bit 0 is set, skip whitespace */
    if ((mode == 0) &&
        (*(uint8_t*)((uint8_t*)child_area + 0x2C) & 1) != 0)  /* +0x2C: flags bit 0 */
    {
        WNDPROC_CriticalSectionUnlock(_this);

        /* Re-check error flags after unlock */
        error_flags = child_area[0x08 / 4];                  /* +0x08 */
        if (error_flags != 0) {
            child_area[0x08 / 4] = error_flags | 2;

            /* Release read CS we just acquired */
            int32_t* child_stream2 = *(int32_t**)((uint8_t*)child_area + 4);
            if (child_stream2 != NULL && child_stream2[0x30 / 4] < 0) {
                WNDPROC_LeaveCriticalSection(
                    (CRITICAL_SECTION*)((uint8_t*)child_stream2 + 0x34));
            }
            goto error_exit;
        }
    }

    return 1;

error_exit:
    /* Release write CS */
    if (child_area[0x34 / 4] < 0) {
        WNDPROC_LeaveCriticalSection(
            (CRITICAL_SECTION*)((uint8_t*)child_area + 0x38));
    }
    return 0;
}


/**
 * WNDPROC_CriticalSectionUnlock — Read and discard whitespace from stream.
 * Address: 0x464B10
 * Size: 173 bytes
 * Calling convention: __fastcall (ECX = _this)
 *
 * Acquires the child's read CS. Reads characters from the stream one
 * by one, discarding all whitespace (isspace). Stops when a non-whitespace
 * character or EOF is found (the non-whitespace char is left in the stream
 * as an unread character). Then sets error flag bit 0 (read-flag) and
 * acquires the write CS. Releases both CS on exit.
 *
 * Used for format parsing: skips whitespace between tokens during reads.
 *
 * Called by:
 *   - WNDPROC_CriticalSectionInit (when mode==0 and flag bit 0 is set)
 *
 * @param _this  StreamInfo block
 */
void __fastcall WNDPROC_CriticalSectionUnlock(void* _this)
{
    int32_t* base = (int32_t*)_this;
    int32_t  child_offset;
    int32_t* child_area;
    int32_t* child_stream;
    int32_t  ch;

    child_offset = *(int32_t*)(base[0x00 / 4] + 4);  /* vtable[1] */
    child_area = (int32_t*)((uint8_t*)_this + child_offset);

    /* Acquire child's read CS */
    child_stream = *(int32_t**)((uint8_t*)child_area + 4);  /* child_area->child */
    if (child_stream != NULL && child_stream[0x30 / 4] < 0) {  /* child->sync_active */
        WNDPROC_EnterCriticalSection(
            (CRITICAL_SECTION*)((uint8_t*)child_stream + 0x34));  /* child->CS */
    }

    /* Read and discard whitespace characters */
    ch = StreamBuf_ReadChar(child_stream);  /* CRT_tan — read char with unget support */
    while (ch != -1) {
        if (!_isspace(ch)) {
            goto done_skip;
        }
        ch = StreamBuf_GetChar(child_stream);  /* CRT_log — read next char */
    }

    /* EOF reached — set error flag */
    {
        int32_t offset2 = *(int32_t*)(base[0x00 / 4] + 4);  /* vtable[1] */
        int32_t flags   = *(int32_t*)((uint8_t*)_this + offset2 + 8);  /* +0x08 */

        /* Acquire write CS */
        if (*(int32_t*)((uint8_t*)_this + offset2 + 0x34) < 0) {  /* sync_active */
            WNDPROC_EnterCriticalSection(
                (CRITICAL_SECTION*)((uint8_t*)_this + offset2 + 0x38));  /* CS */
        }

        /* Set error flag bit 0 (read-flag = EOF) */
        *(int32_t*)((uint8_t*)_this + offset2 + 8) = flags | 1;

        /* Release write CS */
        if (*(int32_t*)((uint8_t*)_this + offset2 + 0x34) < 0) {
            WNDPROC_LeaveCriticalSection(
                (CRITICAL_SECTION*)((uint8_t*)_this + offset2 + 0x38));
        }
    }

done_skip:
    /* Release child's read CS */
    child_stream = *(int32_t**)((uint8_t*)child_area + 4);
    if (child_stream != NULL && child_stream[0x30 / 4] < 0) {
        WNDPROC_LeaveCriticalSection(
            (CRITICAL_SECTION*)((uint8_t*)child_stream + 0x34));
    }
}


/* ================================================================== */
/* Win32 API Wrappers                                                  */
/* ================================================================== */

/**
 * WNDPROC_InitializeCriticalSection — Trivial wrapper for InitializeCriticalSection.
 * Address: 0x464D70
 * Size: 12 bytes
 * Calling convention: __cdecl
 *
 * Delegates to the Win32 API via IAT (import at 0x47710C).
 *
 * Called by:
 *   - WNDPROC_StreamBuf_Ctor (0x4654A6)
 *   - WNDPROC_StreamGetSize (0x4645CD, 0x4645EB)
 *
 * @param cs  Pointer to CRITICAL_SECTION to initialize
 */
void __cdecl WNDPROC_InitializeCriticalSection(CRITICAL_SECTION* cs)
{
    WIN32_InitializeCriticalSection(cs);
}


/**
 * WNDPROC_DeleteCriticalSection — Trivial wrapper for DeleteCriticalSection.
 * Address: 0x464D80
 * Size: 12 bytes
 * Calling convention: __cdecl
 *
 * Delegates to the Win32 API via IAT (import at 0x477110).
 *
 * Called by:
 *   - WNDPROC_StreamBuf_DtorBody (0x4654ED)
 *   - WNDPROC_StreamCleanup (0x464642, 0x46464E)
 *
 * @param cs  Pointer to CRITICAL_SECTION to delete
 */
void __cdecl WNDPROC_DeleteCriticalSection(CRITICAL_SECTION* cs)
{
    WIN32_DeleteCriticalSection(cs);
}


/**
 * WNDPROC_EnterCriticalSection — Trivial wrapper for EnterCriticalSection.
 * Address: 0x464D90
 * Size: 12 bytes
 * Calling convention: __cdecl
 *
 * Previously misidentified as CRT_frexp in Ghidra.
 * Delegates to the Win32 API via IAT (import at 0x477108).
 *
 * @param cs  Pointer to CRITICAL_SECTION to enter
 */
void __cdecl WNDPROC_EnterCriticalSection(CRITICAL_SECTION* cs)
{
    WIN32_EnterCriticalSection(cs);
}


/**
 * WNDPROC_LeaveCriticalSection — Trivial wrapper for LeaveCriticalSection.
 * Address: 0x464DA0
 * Size: 12 bytes
 * Calling convention: __cdecl
 *
 * Previously misidentified as CRT_modf in Ghidra.
 * Delegates to the Win32 API via IAT (import at 0x477104).
 *
 * @param cs  Pointer to CRITICAL_SECTION to leave
 */
void __cdecl WNDPROC_LeaveCriticalSection(CRITICAL_SECTION* cs)
{
    WIN32_LeaveCriticalSection(cs);
}


/* ================================================================== */
/* WIN32 File Stream Functions (file-backed stream I/O)                */
/* ================================================================== */

/**
 * WIN32_StreamRead — Read bytes from stream with synchronization.
 * Address: 0x463810
 * Size: 63 bytes
 * Calling convention: __thiscall
 *
 * Acquires the write critical section, dispatches a read through the
 * child stream's vtable[0x18/4], stores the byte count, and sets
 * error flags on short read. Releases both critical sections.
 *
 * @param _this  Top-level stream object (vtable at +0x00, info at +0x0C)
 * @param buf   Output buffer for read data
 * @param size  Number of bytes to read
 * @return      _this pointer
 */
int32_t* __thiscall WIN32_StreamRead(void* _this, void* buf, uint32_t size)
{
    int32_t result = WNDPROC_CriticalSectionInit(_this, 1);
    if (result != 0) {
        /* Get child stream offset from vtable[1] */
        int32_t child_offset = *(int32_t*)(*(int32_t*)_this + 4);
        void* child_stream = *(void**)((uint8_t*)_this + child_offset + 4);

        /* Call vtable[0x18/4] = Read method on child stream */
        uint32_t bytes_read = ((uint32_t (__thiscall*)(void*, uint32_t))
            (*(void***)child_stream)[0x18 / 4])(buf, size);

        *(uint32_t*)((uint8_t*)_this + 8) = bytes_read;  /* +0x08: store bytes_read */

        if (bytes_read < size) {
            /* Set short read error flags (bits 0 and 1) */
            *(uint32_t*)((uint8_t*)_this + child_offset + 8) |= 3;
        }

        /* Leave critical sections */
        int32_t* cs_base = (int32_t*)((uint8_t*)_this + child_offset);
        if (cs_base[0x30 / 4] < 0) {                    /* child's sync_active */
            WNDPROC_LeaveCriticalSection((CRITICAL_SECTION*)(cs_base + 0x34 / 4));
        }
        if (*(int32_t*)((uint8_t*)_this + child_offset + 0x34) < 0) {
            WNDPROC_LeaveCriticalSection(
                (CRITICAL_SECTION*)((uint8_t*)_this + child_offset + 0x38));
        }
    }

    return (int32_t*)_this;
}


/**
 * WIN32_StreamOpen — Construct a top-level stream wrapper.
 * Address: 0x463890
 * Size: 174 bytes
 * Calling convention: __thiscall
 *
 * If param_1 is non-zero, initializes the stream vtable and info block
 * at +0x0C. Creates a child WIN32_StreamFile (0x54 bytes), attaches it
 * via WNDPROC_StreamVPrintf, sets the child's vtable and mode.
 *
 * SEH-wrapped (implicit in disassembly).
 *
 * @param _this    Top-level stream object (pre-allocated memory)
 * @param param_1 Non-zero = initialize stream header first
 * @return        _this pointer
 */
int32_t* __thiscall WIN32_StreamOpen(void* _this, int32_t param_1)
{
    /* SEH prologue implicit */
    if (param_1 != 0) {
        /* Initialize stream vtable and size tracking */
        *(void**)_this = &DAT_00479188;                 /* +0x00: vtable */
        WNDPROC_StreamGetSize((void*)((uint8_t*)_this + 0x0C));  /* init info block */
    }

    /* Create child WIN32_StreamFile (0x54 bytes) */
    void* child_mem = operator_new(0x54);
    void* child = NULL;
    if (child_mem != NULL) {
        child = WIN32_StreamFile_Ctor(child_mem);
    }

    /* Attach child stream */
    WNDPROC_StreamVPrintf(_this, child, 0);

    /* Set child stream vtable and mode */
    int32_t child_offset = *(int32_t*)(*(int32_t*)_this + 4);  /* vtable[1] */
    *(void***)((uint8_t*)_this + child_offset) = &PTR_LAB_00479184;
    *(int32_t*)((uint8_t*)_this + child_offset + 0x1C) = 1;    /* mode = 1 */

    /* SEH epilogue */
    return (int32_t*)_this;
}


/**
 * WIN32_StreamOpenFile — Open file stream by path.
 * Address: 0x463970
 * Size: 236 bytes
 * Calling convention: __thiscall
 *
 * Creates child WIN32_StreamFile, attaches it, then opens the
 * underlying OS file handle via CRT_exp. Sets error flag on failure.
 *
 * SEH-wrapped (implicit in disassembly).
 *
 * @param _this    Top-level stream object (pre-allocated)
 * @param path    File path to open
 * @param mode    Open mode (combined with flag 1)
 * @param flags   Additional open flags
 * @param param_4 Non-zero = initialize stream header
 * @return        _this pointer
 */
int32_t* __thiscall WIN32_StreamOpenFile(void* _this, const char* path,
                                          uint32_t mode, uint32_t flags,
                                          int32_t param_4)
{
    /* SEH prologue implicit */
    if (param_4 != 0) {
        *(void**)_this = &DAT_00479188;                 /* +0x00: vtable */
        WNDPROC_StreamGetSize((void*)((uint8_t*)_this + 0x0C));  /* init info */
    }

    /* Create child WIN32_StreamFile (0x54 bytes) */
    void* child_mem = operator_new(0x54);
    void* child = NULL;
    if (child_mem != NULL) {
        child = WIN32_StreamFile_Ctor(child_mem);
    }

    /* Attach child stream */
    WNDPROC_StreamVPrintf(_this, child, 0);

    /* Set child stream vtable and mode */
    int32_t child_offset = *(int32_t*)(*(int32_t*)_this + 4);  /* vtable[1] */
    *(void***)((uint8_t*)_this + child_offset) = &PTR_LAB_00479184;
    *(int32_t*)((uint8_t*)_this + child_offset + 0x1C) = 1;    /* mode = 1 */

    /* Open OS file handle via CRT_exp */
    void* child_file_ptr = *(void**)((uint8_t*)_this + child_offset + 4);
    int32_t* file_handle = CRT_exp(child_file_ptr, path, mode | 1, flags);

    if (file_handle == NULL) {
        /* Set error flag bit 1 (file open failure) */
        *(uint32_t*)((uint8_t*)_this + child_offset + 8) |= 2;
    }

    /* SEH epilogue */
    return (int32_t*)_this;
}


/**
 * WIN32_StreamClose — Close top-level stream, flush data, release resources.
 * Address: 0x463A60
 * Size: 20 bytes
 * Calling convention: __fastcall
 *
 * Detaches and flushes the child stream via WNDPROC_StreamPutChar,
 * then releases all stream resources via WNDPROC_StreamCleanup.
 *
 * The info block is at stream + 0x0C.
 *
 * @param stream  Top-level stream object
 */
void __fastcall WIN32_StreamClose(void* stream)
{
    /* Flush trailing data and detach child */
    WNDPROC_StreamPutChar((void*)((uint8_t*)stream + 0x0C));

    /* Release stream resources */
    WNDPROC_StreamCleanup((void*)((uint8_t*)stream + 0x0C));
}
