/**
 * win32_thread.c — WIN32 thread wrapper free functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions manage a Thread object (~0x420 bytes) with a vtable
 * pointer at address 0x479168. The Thread struct layout:
 *
 * struct Thread {
 *     void*    vtable;            // +0x00  -> 0x479168
 *     char     name_buffer[???];  // +0x04..+0x407  name/debug string
 *     uint32_t thread_id;         // +0x408  thread ID
 *     HANDLE   thread_handle;     // +0x40C  CreateThread handle
 *     int32_t  field_410;         // +0x410  callback/arg 1
 *     int32_t  field_414;         // +0x414  callback/arg 2
 *     uint8_t  thread_running;    // +0x418  non-zero = thread active/suspended
 *     // total size: ~0x41C bytes
 * };
 *
 * These are C-linkage functions (not formal C++ methods) that operate
 * on a Thread struct. The "vtable" pattern is a simple dispatch table
 * where slot [0] is the scalar-deleting destructor, slot [1] is ???.
 */

#include <stdint.h>

/* ================================================================== */
/* External Windows API                                                */
/* ================================================================== */

extern void* __stdcall CreateThread(void* security, uint32_t stack_size,
                                     void* start_addr, void* parameter,
                                     uint32_t creation_flags,
                                     uint32_t* thread_id);
extern uint32_t __stdcall WaitForSingleObject(void* handle, uint32_t timeout);
extern uint32_t __stdcall CloseHandle(void* handle);
extern uint32_t __stdcall SetThreadPriority(void* handle, int32_t priority);
extern uint32_t __stdcall ResumeThread(void* handle);
extern uint32_t __stdcall Sleep(uint32_t ms);

/* CRT helpers */
extern void __cdecl GLOBAL_free(void* ptr);

/* Thread vtable at 0x479168 — scalar destructor at slot [0] (WIN32_TerminateThread) */
/* Thread start routine at 0x461890 — internal thread proc */
extern void __stdcall ThreadProc(void* param);

/* Constant strings */
extern const char s_CreateThread_failed[];   /* 0x4818E4 */
extern const char s_Thread_already_active[]; /* 0x4818FC */

/* ================================================================== */
/* Thread struct layout                                                */
/* ================================================================== */
#define THREAD_VTABLE           0x00479168
#define THREAD_OFF_NAME_BUF     0x04
#define THREAD_OFF_THREAD_ID    0x408
#define THREAD_OFF_HANDLE       0x40C
#define THREAD_OFF_FIELD_410    0x410
#define THREAD_OFF_FIELD_414    0x414
#define THREAD_OFF_RUNNING      0x418

/* ================================================================== */
/* WIN32_CreateThread — Initialise Thread struct (constructor)         */
/* Address: 0x461610                                                   */
/* Size: 35 bytes                                                      */
/* Calling convention: __fastcall (param_1 in ECX = Thread*)           */
/*                                                                     */
/* Sets the vtable pointer and zeroes the thread state fields.         */
/* Does NOT actually create a system thread — that's done later by     */
/* WIN32_QueueAsyncTask when it calls CreateThread.                    */
/*                                                                     */
/* @param thread  Thread struct to initialise                           */
/* ================================================================== */
void __fastcall WIN32_CreateThread(void* thread)
{
    uint32_t* p = (uint32_t*)thread;

    p[0] = (uint32_t)THREAD_VTABLE;                          /* +0x00 — vtable */
    p[THREAD_OFF_HANDLE / 4]    = 0;                         /* +0x40C */
    p[THREAD_OFF_FIELD_410 / 4] = 0;                         /* +0x410 */
    p[THREAD_OFF_FIELD_414 / 4] = 0;                         /* +0x414 */
    *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;  /* +0x418 */
}

/* ================================================================== */
/* WIN32_TerminateThread — Scalar deleting destructor for Thread        */
/* Address: 0x461640                                                   */
/* Size: 65 bytes                                                      */
/* Calling convention: __thiscall (this in ECX = Thread*)              */
/*                                                                     */
/* Closes the thread handle if non-NULL, zeroes the running flag.     */
/* If param_1 bit 0 is set, also frees the Thread struct memory.      */
/*                                                                     */
/* This is the vtable[0] scalar deleting destructor for Thread.        */
/*                                                                     */
/* @param flags  Bit 0 = free heap memory (scalar delete pattern)      */
/* @return       this pointer                                         */
/* ================================================================== */
void* __thiscall WIN32_TerminateThread(void* thread, uint8_t flags)
{
    uint32_t* p = (uint32_t*)thread;

    p[0] = THREAD_VTABLE;  /* restore vtable (already set, MSVC convention) */

    if (p[THREAD_OFF_HANDLE / 4] != 0) {
        CloseHandle((void*)p[THREAD_OFF_HANDLE / 4]);
        p[THREAD_OFF_HANDLE / 4] = 0;
        *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;
    }

    if ((flags & 1) != 0) {
        GLOBAL_free(thread);
    }

    return thread;
}

/* ================================================================== */
/* WIN32_WaitForThread — Close thread handle without termination       */
/* Address: 0x461690                                                   */
/* Size: 45 bytes                                                      */
/* Calling convention: __fastcall (param_1 in ECX = Thread*)           */
/*                                                                     */
/* Sets vtable, then closes the handle if non-NULL and zeros running.  */
/* Unlike TerminateThread, does NOT free memory.                       */
/*                                                                     */
/* @param thread  Thread struct                                        */
/* ================================================================== */
void __fastcall WIN32_WaitForThread(void* thread)
{
    uint32_t* p = (uint32_t*)thread;

    p[0] = THREAD_VTABLE;
    if (p[THREAD_OFF_THREAD_ID / 4] != 0) {
        CloseHandle((void*)p[THREAD_OFF_THREAD_ID / 4]);
        p[THREAD_OFF_THREAD_ID / 4] = 0;
        *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;
    }
}

/* ================================================================== */
/* WIN32_ResumeThread — Check if thread is running, set its priority   */
/* Address: 0x4616C0                                                   */
/* Size: 74 bytes                                                      */
/* Calling convention: __thiscall (this in ECX = Thread*)              */
/*                                                                     */
/* Waits on the thread handle with 0ms timeout. If WAIT_TIMEOUT        */
/* (0x102 = still running): returns TRUE and sets thread priority.     */
/* If the thread has exited, clears the running flag and returns 0.    */
/*                                                                     */
/* @param priority  Thread priority to set if still running            */
/* @return          TRUE if thread is still running, FALSE otherwise    */
/* ================================================================== */
uint32_t __thiscall WIN32_ResumeThread(void* thread, int32_t priority)
{
    uint32_t* p = (uint32_t*)thread;
    void* handle = (void*)p[THREAD_OFF_HANDLE / 4];

    if (handle != NULL) {
        uint32_t result = WaitForSingleObject(handle, 0);
        if (result == 0x102) {  /* WAIT_TIMEOUT — thread still running */
            return SetThreadPriority(handle, priority);
        }
        /* Thread exited — clear running flag */
        *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;
    }

    return 0;
}

/* ================================================================== */
/* WIN32_GetThreadResult — Poll whether thread has finished            */
/* Address: 0x461710                                                   */
/* Size: 44 bytes                                                      */
/* Calling convention: __fastcall (param_1 in ECX = Thread*)           */
/*                                                                     */
/* Checks the thread signal state. Returns 0x101 if thread is still    */
/* running, 0 if thread has exited.                                    */
/*                                                                     */
/* @param thread  Thread struct                                        */
/* @return        0x101 = still running, 0 = finished                  */
/* ================================================================== */
uint32_t __fastcall WIN32_GetThreadResult(void* thread)
{
    uint32_t* p = (uint32_t*)thread;
    void* handle = (void*)p[THREAD_OFF_HANDLE / 4];

    if (handle != NULL) {
        uint32_t result = WaitForSingleObject(handle, 0);
        if (result == 0x102) {  /* WAIT_TIMEOUT — thread still active */
            return 0x101;
        }
        *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;
    }

    return 0;
}

/* ================================================================== */
/* WIN32_CloseThreadHandle — Close thread handle, resuming if needed   */
/* Address: 0x461740                                                   */
/* Size: 78 bytes                                                      */
/* Calling convention: __fastcall (param_1 in ECX = Thread*)           */
/*                                                                     */
/* If the thread handle is non-NULL and the thread is still running    */
/* (WAIT_TIMEOUT), checks the running flag. If the running flag is     */
/* set, calls ResumeThread first, then clears it.                      */
/*                                                                     */
/* This is a cleanup helper that handles the "still-running" case.     */
/*                                                                     */
/* @param thread  Thread struct                                        */
/* ================================================================== */
void __fastcall WIN32_CloseThreadHandle(void* thread)
{
    uint32_t* p = (uint32_t*)thread;
    uint32_t still_running = 0;
    void* handle = (void*)p[THREAD_OFF_HANDLE / 4];

    if (handle != NULL) {
        uint32_t result = WaitForSingleObject(handle, 0);
        if (result == 0x102) {  /* WAIT_TIMEOUT — thread still running */
            still_running = 1;
        } else {
            *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;
        }
    }

    if (still_running && *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) != 0) {
        ResumeThread(handle);
        *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;
    }
}

/* ================================================================== */
/* WIN32_QueueAsyncTask — Start thread with callback function          */
/* Address: 0x461790                                                   */
/* Size: 247 bytes                                                     */
/* Calling convention: __thiscall (this in ECX = Thread*)              */
/*                                                                     */
/* Checks if thread is already running (WAIT_TIMEOUT). If so, outputs  */
/* "Thread already active" error message and returns -21 (0xFFFFFFEB). */
/* Otherwise, stores the two callback parameters at +0x410/+0x414 and  */
/* creates a system thread via CreateThread with entry point at        */
/* 0x461890 (ThreadProc). On failure, outputs "CreateThread failed"    */
/* error and returns -22 (0xFFFFFFEA).                                 */
/*                                                                     */
/* @param param1  First parameter to pass to thread proc (stored at +0x414) */
/* @param param2  Second parameter (stored at +0x410)                  */
/* @return        1 on success, -21 if already active, -22 on failure  */
/* ================================================================== */
int32_t __thiscall WIN32_QueueAsyncTask(void* thread,
                                         void* param1,
                                         void* param2)
{
    uint32_t* p = (uint32_t*)thread;
    void* handle = (void*)p[THREAD_OFF_HANDLE / 4];

    if (handle != NULL) {
        uint32_t result = WaitForSingleObject(handle, 0);
        if (result == 0x102) {  /* WAIT_TIMEOUT — thread still running */
            /* Error: thread already active */
            /* Copy error string to name buffer at +0x04 */
            const char* src = s_Thread_already_active;
            char* dst = (char*)((uint8_t*)thread + THREAD_OFF_NAME_BUF);
            while (*src != '\0') {
                *dst++ = *src++;
            }
            *dst = '\0';

            *(uint32_t*)((uint8_t*)thread + 4) = 0xFFFFFFEB;
            return -21;
        }
        *(uint8_t*)((uint8_t*)thread + THREAD_OFF_RUNNING) = 0;
    }

    /* Store callback parameters */
    p[THREAD_OFF_FIELD_414 / 4] = (uint32_t)param2;  /* +0x414 = param1 (note: swapped) */
    p[THREAD_OFF_FIELD_410 / 4] = (uint32_t)param1;  /* +0x410 = param2 */

    /* Create the system thread */
    handle = CreateThread(NULL, 0, (void*)0x461890, thread, 0,
                          (uint32_t*)((uint8_t*)thread + THREAD_OFF_THREAD_ID));
    p[THREAD_OFF_HANDLE / 4] = (uint32_t)handle;

    if (handle != NULL) {
        return 1;  /* success */
    }

    /* Error: CreateThread failed */
    const char* src = s_CreateThread_failed;
    char* dst = (char*)((uint8_t*)thread + THREAD_OFF_NAME_BUF);
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';

    return -22;
}
