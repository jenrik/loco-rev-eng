/**
 * crt_stubs.cpp — CRT function reference for loco.exe
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file documents every CRT function statically linked into loco.exe
 * from the MSVC 6.0 runtime library. These functions exist in the
 * 0x464EF0–0x4767D6 address range and represent the entire CRT surface
 * that any reimplementation must provide.
 *
 * Each entry specifies:
 *   - Address range (start, end, size in bytes)
 *   - Standard CRT function name it implements
 *   - The MSVC 6.0 object file / library it was linked from
 *   - Calling convention and signature (where determined)
 *   - Which game subsystems call it (when known)
 *
 * Total: approximately 230 CRT functions across ~120KB of .text.
 */

#include "crt_stubs.h"

// clang-format off

/* ================================================================== */
/*  NOTICE                                                             */
/* ================================================================== */
/*
 * These CRT functions are standard MSVC 6.0 runtime implementations.
 * They are NOT application-specific. A reimplementation should link
 * against an equivalent CRT library rather than reimplementing these.
 *
 * The documentation here exists to:
 *   1. Provide a complete inventory of every CRT function in the binary
 *   2. Enable accurate Ghidra function naming for reverse engineering
 *   3. Support reimplementation planning (which CRT object files are needed)
 *
 * Functions are organized by CRT object file where known. The MSVC 6.0
 * CRT linked into loco.exe used the LIBC.LIB static library (single-threaded)
 * with some functions from LIBCMT.LIB (multi-threaded) for the DPlay thread.
 *
 * Object files commonly found in MSVC 6.0 static CRT:
 *   _FILE.OBJ  — Stream I/O infrastructure
 *   _FILBUF.OBJ — fgetc / fscanf buffer operations
 *   _FLSBUF.OBJ — fputc buffer flush
 *   _FTOL.OBJ  — float-to-long conversion
 *   _STB.OBJ   — setvbuf/setbuf
 *   _STRXFRM.OBJ — strxfrm
 *   _TOLOWER.OBJ — tolower/toupper
 *   _WSTRLEN.OBJ — wcslen
 *   _WSTRSYS.OBJ — wcscmp, wcscat, etc.
 *   _WSTROPS.OBJ — wcschr, wcsrchr, wcsstr, etc.
 *   _WINXFLTR.OBJ — C++ exception handling
 *   _WINXFRM.OBJ — _CxxFrameHandler
 *   _WINXPHTR.OBJ — _purecall
 *   XMATH.OBJ  — sin, cos, tan, exp, log, fmod, etc.
 *   _NEW.OBJ   — operator new/delete
 *   _HEAP.OBJ  — heap management
 *   _MALLOC.OBJ — malloc/free
 *   _INIT.OBJ  — CRT init/startup
 *   _EXIT.OBJ  — exit/abort
 *   _STRTOUL.OBJ — wcstol, wcstoul, wcstod
 *   _TIME.OBJ  — time, localtime, strftime
 *   _LOCALE.OBJ — locale/ctype tables
 */

/* ================================================================== */
/*  RANGE: 0x463C30 – 0x465FFF — Win32 Platform Layer (not CRT)       */
/* ================================================================== */
/*
 * The range 0x463C30–0x465FFF contains the Win32 platform layer:
 *   - WIN32_Stream*:     File/memory stream abstraction
 *   - WNDPROC_Stream*:   Stream operations bound to WndProc
 *   - CRT_fmod, CRT_fab: Math library (from MSVCRT math library object files)
 *
 * These are documented in their respective subsystem files under
 * src/decompiled_cpp/native/ (Win32 platform) or are math functions
 * listed in the math section below.
 *
 * Math functions (0x464EF0–0x4655B8):
 *   fmod, fabs, ceil, floor, log10, log, cos, sin, tan, exp, atan, acos
 *   src: XMATH.OBJ from MSVC 6.0 LIBC.LIB
 */

/* ================================================================== */
/*  0x464EF0 — fmod                                                    */
/* ================================================================== */
/*  Standard C fmod(x, y) — floating-point remainder.
 *  Address: 0x464EF0, size: 120 bytes
 *  Calling convention: __cdecl
 *  Called by: Game_LoadWaveFile, audio positioning
 *  Objects: floats at +0x18, +0x1C on stack
 */

/* ================================================================== */
/*  0x464F70 — fabs                                                    */
/* ================================================================== */
/*  Standard C fabs(x) — absolute value.
 *  Address: 0x464F70, size: 148 bytes
 *  Calling convention: __cdecl
 *  Called by: Game loop, math_helpers
 */

/* ================================================================== */
/*  0x465010 — WIN32 stream write (NOT ceil)                           */
/* ================================================================== */
/*  See the 0x465010/0x465090 note below: this address is the WIN32    */
/*  stream WRITE function, not CRT ceil (mislabel corrected).          */

/* ================================================================== */
/*  0x465010 / 0x465090 — WIN32 stream functions, NOT CRT ceil/floor   */
/* ================================================================== */
/*  objdump of loco.exe proves these addresses are the WIN32 stream    */
/*  WRITE function (0x465010: thiscall(buf, size), ret $0x8, called    */
/*  from RESMGR_LoadResourceData 0x447ED6/0x447EFC) and the WRITE-     */
/*  stream constructor (0x465090: thiscall(path, mode, flags, flag),   */
/*  mode|2, called from 0x447E8F).  The "ceil"/"floor" labels were     */
/*  decompiler misidentifications; the real stream functions are       */
/*  declared in resources/ResDataSave.cpp (WIN32_StreamWrite /         */
/*  WIN32_StreamOpenWriteFile). */

/* ================================================================== */
/*  0x465180 — log10                                                   */
/* ================================================================== */
/*  Standard C log10(x) — base-10 logarithm.
 *  Address: 0x465180, size: 19 bytes
 *  Calling convention: __cdecl
 *  Thunk to 0x4651A0 (log).
 */

/* ================================================================== */
/*  0x4651A0 — log                                                     */
/* ================================================================== */
/*  Standard C log(x) — natural logarithm.
 *  Address: 0x4651A0, size: 85 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x465200 — cos                                                     */
/* ================================================================== */
/*  Standard C cos(x) — cosine.
 *  Address: 0x465200, size: 75 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x465250 — sin                                                     */
/* ================================================================== */
/*  Standard C sin(x) — sine.
 *  Address: 0x465250, size: 67 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4652A0 — tan                                                     */
/* ================================================================== */
/*  Standard C tan(x) — tangent.
 *  Address: 0x4652A0, size: 38 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4652D0 — exp                                                     */
/* ================================================================== */
/*  Standard C exp(x) — exponential function.
 *  Address: 0x4652D0, size: 414 bytes
 *  Calling convention: __cdecl
 *  Uses __fpmath extended precision support.
 */

/* ================================================================== */
/*  0x4654C0 — atan                                                    */
/* ================================================================== */
/*  Standard C atan(x) — arctangent.
 *  Address: 0x4654C0, size: 30 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x465560 — acos                                                    */
/* ================================================================== */
/*  Standard C acos(x) — arccosine.
 *  Address: 0x465560, size: 88 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4656B0 — _cexit                                                  */
/* ================================================================== */
/*  CRT cleanup on exit — calls registered atexit handlers.
 *  Address: 0x4656B0, size: 27 bytes
 *  Calling convention: __cdecl
 *  Called from: _mainCRTStartup exit path
 *  src: _EXIT.OBJ
 */

/* ================================================================== */
/*  0x4656F0 — _exit_handler                                           */
/* ================================================================== */
/*  CRT exit handler dispatcher — iterates atexit callback table.
 *  Address: 0x4656F0, size: 49 bytes
 *  Calling convention: __cdecl
 *  src: _EXIT.OBJ
 */

/* ================================================================== */
/*  0x4657A0 — _flsbuf                                                 */
/* ================================================================== */
/*  Flushes a FILE write buffer then writes a character. Called when
 *  the buffer is full during fputc/fwrite.
 *  Address: 0x4657A0, size: 105 bytes
 *  Calling convention: __thiscall
 *  Called by: CRT_fputc, CRT_fwrite, CRT_fprintf internals
 *  src: _FLSBUF.OBJ
 */

/* ================================================================== */
/*  0x465810 — _fflush_nolock                                          */
/* ================================================================== */
/*  Flushes a FILE stream's write buffer without locking. Called
 *  internally from fflush, fclose, etc.
 *  Address: 0x465810, size: 127 bytes
 *  Calling convention: __fastcall
 *  Called by: CRT_fclose, CRT_fflush, CRT_fseek
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x465890 — sqrtf                                                   */
/* ================================================================== */
/*  Standard C sqrtf(x) — float square root.
 *  Address: 0x465890, size: 202 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x465960 — _ftolf                                                  */
/* ================================================================== */
/*  float-to-long conversion with truncation toward zero.
 *  Address: 0x465960, size: 153 bytes
 *  Calling convention: __cdecl
 *  src: _FTOL.OBJ
 */

/* ================================================================== */
/*  0x465A30 — _except_handler                                         */
/* ================================================================== */
/*  CRT exception handler for SEH — top-level filter for C exceptions.
 *  Address: 0x465A30, size: 133 bytes
 *  Calling convention: __cdecl
 *  src: _WINXFLTR.OBJ
 */

/* ================================================================== */
/*  0x465AC0 — _seh_filter                                             */
/* ================================================================== */
/*  SEH filter — returns EXCEPTION_EXECUTE_HANDLER for C exceptions.
 *  Address: 0x465AC0, size: 15 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x465AD0 — _ftol                                                   */
/* ================================================================== */
/*  double-to-long conversion with truncation (used by compiler).
 *  Address: 0x465AD0, size: 511 bytes
 *  Calling convention: __cdecl
 *  Called by: compiler-generated code for float→int casts
 *  src: _FTOL.OBJ
 */

/* ================================================================== */
/*  0x465CD0 — GLOBAL_free                                             */
/* ================================================================== */
/*  Game-specific global free wrapper — calls CRT_free.
 *  Address: 0x465CD0, size: 14 bytes
 *  Calling convention: __cdecl
 *  NOT standard CRT — game code convenience wrapper.
 */

/* ================================================================== */
/*  0x465CE0 — operator new                                            */
/* ================================================================== */
/*  C++ operator new(size_t) — calls malloc.
 *  Address: 0x465CE0, size: 16 bytes
 *  Calling convention: __cdecl
 *  src: _NEW.OBJ
 */

/* ================================================================== */
/*  0x465CF0 — CRT_operator_new                                        */
/* ================================================================== */
/*  C++ operator new(size_t) with nothrow semantics.
 *  Address: 0x465CF0, size: 45 bytes
 *  Calling convention: __cdecl
 *  src: _NEW.OBJ
 */

/* ================================================================== */
/*  0x465D30 — CRT_operator_delete                                     */
/* ================================================================== */
/*  C++ operator delete(void*) — calls free.
 *  Address: 0x465D30, size: 7 bytes
 *  Calling convention: __cdecl
 *  src: _NEW.OBJ
 */

/* ================================================================== */
/*  0x465D40 — _purecall                                               */
/* ================================================================== */
/*  Pure virtual function call handler — called when a pure virtual
 *  function is invoked. Calls abort().
 *  Address: 0x465D40, size: 86 bytes
 *  Calling convention: __cdecl
 *  src: _WINXPHTR.OBJ
 */

/* ================================================================== */
/*  0x465DA0 — _amsg_exit                                              */
/* ================================================================== */
/*  CRT message on exit — displays runtime error and terminates.
 *  Address: 0x465DA0, size: 60 bytes
 *  Calling convention: __cdecl
 *  src: _AMSG.OBJ
 */

/* ================================================================== */
/*  0x465DE0 — GLOBAL_alloc                                            */
/* ================================================================== */
/*  Game-specific global alloc wrapper — calls CRT_malloc.
 *  Address: 0x465DE0, size: 91 bytes
 *  Calling convention: __cdecl
 *  NOT standard CRT — game code convenience wrapper with NULL checking.
 */

/* ================================================================== */
/*  0x465E40 — __CxxExceptionFilter                                    */
/* ================================================================== */
/*  C++ exception filter — SEH filter for C++ exceptions.
 *  Address: 0x465E40, size: 42 bytes
 *  Calling convention: __cdecl
 *  src: _WINXFLTR.OBJ
 */

/* ================================================================== */
/*  0x465E70 — powf                                                    */
/* ================================================================== */
/*  Standard C powf(x, y) — float exponentiation.
 *  Address: 0x465E70, size: 204 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x465F40 — _initterm                                               */
/* ================================================================== */
/*  CRT initializer — calls function pointers in a table range.
 *  Used for C++ static initializers.
 *  Address: 0x465F40, size: 124 bytes
 *  Calling convention: __cdecl
 *  src: _INIT.OBJ
 */

/* ================================================================== */
/*  0x465FD0 — _initterm_e                                             */
/* ================================================================== */
/*  CRT initializer (error-returning variant) — calls function pointers
 *  in a table range, stopping on error.
 *  Address: 0x465FD0, size: 127 bytes
 *  Calling convention: __cdecl
 *  src: _INIT.OBJ
 */

/* ================================================================== */
/*  RANGE: 0x466000 — 0x46613E — C++ Exception Handling Helpers       */
/* ================================================================== */

/* ================================================================== */
/*  0x466050 — __global_unwind2                                        */
/* ================================================================== */
/*  SEH global unwind — unwinds frames until reaching a target.
 *  Address: 0x466050, size: 32 bytes
 *  Calling convention: __cdecl
 *  src: _WINXFLTR.OBJ
 */

/* ================================================================== */
/*  0x466092 — __local_unwind2                                         */
/* ================================================================== */
/*  SEH local unwind — unwinds local frames with destructor calls.
 *  Address: 0x466092, size: 104 bytes
 *  Calling convention: __cdecl
 *  src: _WINXFLTR.OBJ
 */

/* ================================================================== */
/*  0x4660FA — __abnormal_termination                                  */
/* ================================================================== */
/*  Returns non-zero if during an SEH unwind (not a normal return).
 *  Address: 0x4660FA, size: 35 bytes
 *  Calling convention: __cdecl
 *  src: _WINXFLTR.OBJ
 */

/* ================================================================== */
/*  0x46611D — __NLG_Notify1                                           */
/* ================================================================== */
/*  Stores current ESP for non-local-goto (setjmp/longjmp) tracking.
 *  Address: 0x46611D, size: 9 bytes
 *  Calling convention: __cdecl
 *  src: _WINXFLTR.OBJ
 */

/* ================================================================== */
/*  0x466126 — __NLG_Notify2                                           */
/* ================================================================== */
/*  Stores SEH exception context (EAX, EBP, return address) into
 *  globals for non-local-goto tracking across destructors.
 *  Address: 0x466126, size: 24 bytes
 *  Calling convention: __cdecl
 *  src: _WINXFLTR.OBJ
 */

/* ================================================================== */
/*  0x466140 — srand (small)                                           */
/* ================================================================== */
/*  Standard C srand(unsigned seed) — seed PRNG.
 *  Address: 0x466140, size: 13 bytes
 *  Calling convention: __cdecl
 *  Thunk to internal srand implementation.
 */

/* ================================================================== */
/*  0x466150 — rand (small)                                            */
/* ================================================================== */
/*  Standard C rand(void) — returns pseudo-random int.
 *  Address: 0x466150, size: 45 bytes
 *  Calling convention: __cdecl
 *  src: _RAND.OBJ
 */

/* ================================================================== */
/*  0x466180 — __fpmath                                                */
/* ================================================================== */
/*  FPU initialization helper — sets the FPU control word for
 *  extended precision.
 *  Address: 0x466180, size: 23 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4661A0 — alignment filler                                        */
/* ================================================================== */
/*  NOT a real function — 1-byte alignment filler between sections.
 *  Address: 0x4661A0, size: 1 byte
 *  Never called.
 */

/* ================================================================== */
/*  0x4661B0 — ___lc_locale_init                                       */
/* ================================================================== */
/*  Initializes function pointers for locale-aware string comparison
 *  operations (strcoll, strxfrm, wcscoll, wcsxfrm).
 *  Address: 0x4661B0, size: 56 bytes
 *  Calling convention: __cdecl
 *  src: _LOCALE.OBJ
 */

/* ================================================================== */
/*  0x4661F0 — _strncpy                                                */
/* ================================================================== */
/*  Standard strncpy — copies at most count chars, null-pads remainder.
 *  Address: 0x4661F0, size: 254 bytes
 *  Calling convention: __cdecl
 *  src: _STRNCPY.OBJ
 */

/* ================================================================== */
/*  0x4662F0 — _atoi_l                                                 */
/* ================================================================== */
/*  atoi with locale — locale-aware string-to-int conversion with
 *  whitespace skipping and sign handling. Used when locale is active.
 *  Address: 0x4662F0, size: 153 bytes
 *  Calling convention: __cdecl
 *  src: _ATOI.OBJ
 */

/* ================================================================== */
/*  0x466390 — atoi                                                    */
/* ================================================================== */
/*  Standard atoi — thunk to _atoi_l or direct ASCII-to-int.
 *  Address: 0x466390, size: 14 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4663A0 — strtok                                                  */
/* ================================================================== */
/*  Standard strtok — tokenizes string, using internal static state.
 *  Address: 0x4663A0, size: 232 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x466490 — time                                                    */
/* ================================================================== */
/*  Standard C time(time_t*) — get seconds since epoch.
 *  Address: 0x466490, size: 47 bytes
 *  Calling convention: __cdecl
 *  Calls GetSystemTime + SystemTimeToFileTime + maths.
 *  src: _TIME.OBJ
 */

/* ================================================================== */
/*  0x4664C0 — difftime                                                */
/* ================================================================== */
/*  Standard C difftime(t1, t0) — double difference in seconds.
 *  Address: 0x4664C0, size: 194 bytes
 *  Calling convention: __cdecl
 *  src: _TIME.OBJ
 */

/* ================================================================== */
/*  0x466590 — _mkdir                                                  */
/* ================================================================== */
/*  Creates a directory via CreateDirectoryA. Handles UNC paths via
 *  _isunccpath check.
 *  Address: 0x466590, size: 831 bytes
 *  Calling convention: __cdecl
 *  src: _MKDIR.OBJ
 */

/* ================================================================== */
/*  0x4668D0 — _isunccpath                                             */
/* ================================================================== */
/*  Checks if a path string is a UNC path (starts with \\ or //,
 *  followed by server and share components).
 *  Address: 0x4668D0, size: 127 bytes
 *  Calling convention: __cdecl
 *  Called by: _mkdir
 *  src: _MKDIR.OBJ (internal helper)
 */

/* ================================================================== */
/*  0x466950 — locked sprintf_s wrapper                                */
/* ================================================================== */
/*  Wrapper around sprintf_s that acquires/releases a lock (index 0xC).
 *  Address: 0x466950, size: 47 bytes
 *  Calling convention: __cdecl
 *  src: _SPRINTF.OBJ
 */

/* ================================================================== */
/*  0x466980 — sprintf_s                                               */
/* ================================================================== */
/*  Secure sprintf — writes formatted data to a buffer with size limit.
 *  Address: 0x466980, size: 299 bytes
 *  Calling convention: __cdecl
 *  src: _SPRINTF_S.OBJ
 */

/* ================================================================== */
/*  0x466AB0 — _isvaliddrive                                           */
/* ================================================================== */
/*  Checks if a drive letter (A:, B:, etc.) corresponds to an existing
 *  drive via GetDriveTypeA.
 *  Address: 0x466AB0, size: 64 bytes
 *  Calling convention: __cdecl
 *  src: _MKDIR.OBJ (internal helper)
 */

/* ================================================================== */
/*  0x466AF0 — timeGetTime wrapper                                     */
/* ================================================================== */
/*  Wraps timeGetTime (Win32 multimedia timer) for high-resolution
 *  timing. NOT standard CRT — game code.
 *  Address: 0x466AF0, size: 300 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x466C20 — type_info::~type_info                                   */
/* ================================================================== */
/*  RTTI type_info destructor — resets vtable, frees name string.
 *  Address: 0x466C20, size: 47 bytes
 *  Calling convention: __stdcall
 *  src: _TINFO.OBJ
 */

/* ================================================================== */
/*  0x466C50 — type_info scalar deleting destructor                    */
/* ================================================================== */
/*  MSVC scalar deleting destructor for type_info objects.
 *  Address: 0x466C50, size: 30 bytes
 *  Calling convention: __thiscall
 *  src: _TINFO.OBJ
 */

/* ================================================================== */
/*  0x466C70 — free                                                    */
/* ================================================================== */
/*  Standard C free(void*) — frees heap-allocated memory via HeapFree
 *  on the CRT private heap.
 *  Address: 0x466C70, size: 104 bytes
 *  Calling convention: __cdecl
 *  src: _FREE.OBJ
 */

/* ================================================================== */
/*  0x466CE0 — exit                                                    */
/* ================================================================== */
/*  Standard C exit(int code) — calls _cexit, then ExitProcess.
 *  Address: 0x466CE0, size: 71 bytes
 *  Calling convention: __cdecl
 *  src: _EXIT.OBJ
 */

/* ================================================================== */
/*  0x466D30 — __ftol                                                  */
/* ================================================================== */
/*  Compiler helper: double-to-long truncation. Called by compiler
 *  for float-to-int casts.
 *  Address: 0x466D30, size: 39 bytes
 *  Calling convention: __cdecl
 *  src: _FTOL.OBJ
 */

/* ================================================================== */
/*  0x466D60 — vsprintf / sprintf_buf                                  */
/* ================================================================== */
/*  Formats a string with va_list arguments (non-secure).
 *  Address: 0x466D60, size: 104 bytes
 *  Calling convention: __cdecl
 *  src: _SPRINTF.OBJ
 */

/* ================================================================== */
/*  0x466DE0 — malloc                                                  */
/* ================================================================== */
/*  Standard C malloc(size_t) — allocates memory from CRT private heap.
 *  Falls back to _nh_malloc for small blocks.
 *  Address: 0x466DE0, size: 193 bytes
 *  Calling convention: __cdecl
 *  src: _MALLOC.OBJ
 */

/* ================================================================== */
/*  0x466EA0 — strncpy (large)                                         */
/* ================================================================== */
/*  Full implementation of strncpy with buffer overflow check and
 *  null-padding. Larger than the minimal stub at 0x4661F0.
 *  Address: 0x466EA0, size: 664 bytes
 *  Calling convention: __cdecl
 *  src: _STRNCPY.OBJ
 */

/* ================================================================== */
/*  0x4671E0 — CRT_memset_pattern                                      */
/* ================================================================== */
/*  MSVC-specific memset variant that fills a buffer with a repeating
 *  4-byte pattern. Used for initializing arrays of structures.
 *  Address: 0x4671E0, size: 111 bytes
 *  Calling convention: __cdecl
 *  src: _MEMSET.OBJ
 */

/* ================================================================== */
/*  0x467258 — qsort comparison helper                                 */
/* ================================================================== */
/*  Internal callback for qsort byte comparisons. Calls CRT_strncmp
 *  when a comparison flag is zero.
 *  Address: 0x467258, size: 20 bytes
 *  Calling convention: __cdecl
 *  src: _QSORT.OBJ
 */

/* ================================================================== */
/*  0x467280 — CRT_free_pattern                                        */
/* ================================================================== */
/*  Frees memory allocated by CRT_memset_pattern. Reverse of
 *  the pattern allocator.
 *  Address: 0x467280, size: 115 bytes
 *  Calling convention: __cdecl
 *  src: _MEMSET.OBJ (pattern allocator)
 */

/* ================================================================== */
/*  0x4672F9 — qsort comparison helper (alternate)                     */
/* ================================================================== */
/*  Secondary comparison callback for qsort internals.
 *  Address: 0x4672F9, size: 23 bytes
 *  Calling convention: __cdecl
 *  src: _QSORT.OBJ
 */

/* ================================================================== */
/*  0x467330 — strncmp (standard)                                      */
/* ================================================================== */
/*  Standard C strncmp — compares two strings up to n characters.
 *  Address: 0x467330, size: 90 bytes
 *  Calling convention: __cdecl
 *  src: _STRNCMP.OBJ
 */

/* ================================================================== */
/*  0x4673C0 — malloc_zero                                             */
/* ================================================================== */
/*  Calls CRT_malloc followed by zero-fill. Allocates zero-initialized
 *  memory.
 *  Address: 0x4673C0, size: 20 bytes
 *  Calling convention: __cdecl
 *  src: _MALLOC.OBJ
 */

/* ================================================================== */
/*  0x4673E0 — _malloc_base                                            */
/* ================================================================== */
/*  Core malloc implementation with new_handler retry support.
 *  Retries allocation via _callnewh if param_2 is set.
 *  Address: 0x4673E0, size: 68 bytes
 *  Calling convention: __cdecl
 *  src: _MALLOC.OBJ
 */

/* ================================================================== */
/*  0x467430 — _nh_malloc                                              */
/* ================================================================== */
/*  Small-block allocator for CRT heap. Attempts allocation from
 *  small-block cache, falls back to HeapAlloc on CRT private heap.
 *  Address: 0x467430, size: 85 bytes
 *  Calling convention: __cdecl
 *  src: _NHMALLOC.OBJ
 */

/* ================================================================== */
/*  0x467490 — _CrtDbgReport format helper                             */
/* ================================================================== */
/*  Debug format helper with 'I' specifier handling. Used by
 *  _CrtDbgReport or similar VC debug CRT functions.
 *  Address: 0x467490, size: 66 bytes
 *  Calling convention: __cdecl
 *  src: _DBGRPT.OBJ
 */

/* ================================================================== */
/*  0x4674E0 — localtime                                               */
/* ================================================================== */
/*  Standard C localtime(time_t*) — converts time to broken-down
 *  struct tm with timezone adjustment and DST check.
 *  Address: 0x4674E0, size: 486 bytes
 *  Calling convention: __cdecl
 *  Called by: Building_DecideAction (schedule checking)
 *  src: _TIME.OBJ
 */

/* ================================================================== */
/*  0x4676D0 — _strpbrk                                                */
/* ================================================================== */
/*  Standard strpbrk — finds first character match from charset.
 *  Address: 0x4676D0, size: 58 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x467710 — toupper (locale-aware)                                  */
/* ================================================================== */
/*  Converts a character to uppercase using locale ctype table
 *  with thread-safe lock.
 *  Address: 0x467710, size: 130 bytes
 *  Calling convention: __cdecl
 *  src: _TOLOWER.OBJ
 */

/* ================================================================== */
/*  0x4677A0 — strncpy_s                                               */
/* ================================================================== */
/*  Secure strncpy_s — copies string with bounds checking.
 *  Address: 0x4677A0, size: 254 bytes
 *  Calling convention: __cdecl
 *  src: _STRNCPY_S.OBJ
 */

/* ================================================================== */
/*  0x4678A0 — strncat_s                                               */
/* ================================================================== */
/*  Secure strncat_s — appends string with bounds checking.
 *  Address: 0x4678A0, size: 369 bytes
 *  Calling convention: __cdecl
 *  src: _STRNCAT_S.OBJ
 */

/* ================================================================== */
/*  0x467A20 — _findfirst                                              */
/* ================================================================== */
/*  CRT _findfirst — wraps FindFirstFileA, returns search handle.
 *  Address: 0x467A20, size: 257 bytes
 *  Calling convention: __cdecl
 *  src: _FINDFILE.OBJ
 */

/* ================================================================== */
/*  0x467B50 — _findnext                                               */
/* ================================================================== */
/*  CRT _findnext — wraps FindNextFileA.
 *  Address: 0x467B50, size: 249 bytes
 *  Calling convention: __cdecl
 *  src: _FINDFILE.OBJ
 */

/* ================================================================== */
/*  0x467C70 — _findclose                                              */
/* ================================================================== */
/*  CRT _findclose — wraps FindClose.
 *  Address: 0x467C70, size: 33 bytes
 *  Calling convention: __cdecl
 *  src: _FINDFILE.OBJ
 */

/* ================================================================== */
/*  0x467CA0 — _filetime_to_time_t                                     */
/* ================================================================== */
/*  Converts FILETIME struct to seconds since 1970 (time_t) via
 *  FileTimeToLocalFileTime + FileTimeToSystemTime + mktime math.
 *  Address: 0x467CA0, size: 140 bytes
 *  Calling convention: __cdecl
 *  src: _TIME.OBJ
 */

/* ================================================================== */
/*  0x467D30 — _strncat                                                */
/* ================================================================== */
/*  Standard strncat — appends at most n characters.
 *  Address: 0x467D30, size: 291 bytes
 *  Calling convention: __cdecl
 *  src: _STRNCAT.OBJ
 */

/* ================================================================== */
/*  0x467E60 — _strrchr                                                */
/* ================================================================== */
/*  Standard strrchr — finds last occurrence of a character.
 *  Address: 0x467E60, size: 39 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x467EA0 — itoa                                                    */
/* ================================================================== */
/*  Standard itoa — integer to ASCII (radix support).
 *  Address: 0x467EA0, size: 61 bytes
 *  Calling convention: __cdecl
 *  src: _ITOA.OBJ
 */

/* ================================================================== */
/*  0x467EE0 — ltoa                                                    */
/* ================================================================== */
/*  Standard ltoa — long to ASCII (radix support).
 *  Address: 0x467EE0, size: 97 bytes
 *  Calling convention: __cdecl
 *  src: _ITOA.OBJ
 */

/* ================================================================== */
/*  0x467F50 — ultoa                                                   */
/* ================================================================== */
/*  Standard ultoa — unsigned long to ASCII (radix support).
 *  Address: 0x467F50, size: 115 bytes
 *  Calling convention: __cdecl
 *  src: _ITOA.OBJ
 */

/* ================================================================== */
/*  0x467FD0 — _errno                                                  */
/* ================================================================== */
/*  Returns pointer to thread-local errno value.
 *  Address: 0x467FD0, size: 9 bytes
 *  Calling convention: __cdecl
 *  src: _ERRNO.OBJ
 */

/* ================================================================== */
/*  0x467FE0 — __doserrno                                              */
/* ================================================================== */
/*  Returns pointer to thread-local DOS error code.
 *  Address: 0x467FE0, size: 9 bytes
 *  Calling convention: __cdecl
 *  src: _ERRNO.OBJ
 */

/* ================================================================== */
/*  0x467FF0 — _vsnprintf                                              */
/* ================================================================== */
/*  vsnprintf variant — formats into fixed-size buffer with
 *  null-termination.
 *  Address: 0x467FF0, size: 104 bytes
 *  Calling convention: __cdecl
 *  src: _SPRINTF.OBJ
 */

/* ================================================================== */
/*  0x468060 — _strstr                                                 */
/* ================================================================== */
/*  Standard strstr — finds substring in string.
 *  Address: 0x468060, size: 128 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4680E0 — _ungetwc internal buffer                                 */
/* ================================================================== */
/*  Manages the pushback buffer for ungetwc operations. Expands buffer
 *  via realloc when needed.
 *  Address: 0x4680E0, size: 143 bytes
 *  Calling convention: __cdecl
 *  src: _UNGETWC.OBJ
 */

/* ================================================================== */
/*  0x468170 — ungetwc success check                                   */
/* ================================================================== */
/*  Calls CRT_0x4680E0 (buffer push), returns -1 on success or 0 on
 *  failure.
 *  Address: 0x468170, size: 21 bytes
 *  Calling convention: __cdecl
 *  src: _UNGETWC.OBJ
 */

/* ================================================================== */
/*  0x4681D0 — _fflush (locked)                                        */
/* ================================================================== */
/*  Acquires file lock, calls internal flush, releases lock.
 *  Handles wide/narrow stream flush.
 *  Address: 0x4681D0, size: 61 bytes
 *  Calling convention: __cdecl
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x468210 — _fflush_nolock (internal)                               */
/* ================================================================== */
/*  Internal fflush — flushes FILE write buffer, calls _close on
 *  underlying fd if needed.
 *  Address: 0x468210, size: 101 bytes
 *  Calling convention: __cdecl
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x468280 — _fflush (locked, external)                              */
/* ================================================================== */
/*  Locks the FILE, calls internal flush, unlocks. Handles NULL
 *  stream by flushing all streams.
 *  Address: 0x468280, size: 55 bytes
 *  Calling convention: __cdecl
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x4682C0 — _fflush_nolock internal                                 */
/* ================================================================== */
/*  Flushes the FILE's write buffer. Calls CRT_0x468300 to do the
 *  actual write.
 *  Address: 0x4682C0, size: 53 bytes
 *  Calling convention: __cdecl
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x468300 — _flush_write_buffer                                     */
/* ================================================================== */
/*  Writes buffered data from FILE stream to OS handle via WriteFile.
 *  Address: 0x468300, size: 110 bytes
 *  Calling convention: __cdecl
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x468380 — wcstok                                                  */
/* ================================================================== */
/*  Standard wcstok — wide char tokenizer.
 *  Address: 0x468380, size: 187 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x468440 — scanf buffer allocator                                  */
/* ================================================================== */
/*  Allocates a buffer for scanf formatted input operations.
 *  Address: 0x468440, size: 55 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x468480 — scanf buffer alloc (64-byte)                            */
/* ================================================================== */
/*  Convenience wrapper for CRT_0x468440 with size 0x40 (64 bytes).
 *  Address: 0x468480, size: 21 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4684A0 — _CRT_INIT                                               */
/* ================================================================== */
/*  CRT initialization — calls __fpmath init callback, then registers
 *  CRT destructors via _initterm.
 *  Address: 0x4684A0, size: 48 bytes
 *  Calling convention: __cdecl
 *  src: _INIT.OBJ
 */

/* ================================================================== */
/*  0x4684D0 — CRT lock wrapper                                        */
/* ================================================================== */
/*  Calls wcsdup with zero parameters — likely dead code or debug
 *  leftover.
 *  Address: 0x4684D0, size: 18 bytes
 *  Calling convention: __cdecl
 *  src: likely _DBG.OBJ
 */

/* ================================================================== */
/*  0x4684F0 — __exit                                                  */
/* ================================================================== */
/*  Calls ExitProcess. Called by exit().
 *  Address: 0x4684F0, size: 18 bytes
 *  Calling convention: __cdecl
 *  src: _EXIT.OBJ
 */

/* ================================================================== */
/*  0x468510 — wcsdup                                                  */
/* ================================================================== */
/*  Standard wcsdup — allocates copy of wide string via malloc.
 *  Address: 0x468510, size: 174 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x4685D0 — _lock_ungetc                                            */
/* ================================================================== */
/*  Acquires CRT lock index 13 (used by ungetc/ungetwc operations).
 *  Address: 0x4685D0, size: 11 bytes
 *  Calling convention: __cdecl
 *  src: _LOCK.OBJ
 */

/* ================================================================== */
/*  0x4685E0 — _unlock_ungetc                                          */
/* ================================================================== */
/*  Releases CRT lock index 13.
 *  Address: 0x4685E0, size: 11 bytes
 *  Calling convention: __cdecl
 *  src: _LOCK.OBJ
 */

/* ================================================================== */
/*  0x4685F0 — _initterm helper                                        */
/* ================================================================== */
/*  Iterates function pointer table range, calling non-NULL entries.
 *  Address: 0x4685F0, size: 32 bytes
 *  Calling convention: __cdecl
 *  src: _INIT.OBJ
 */

/* ================================================================== */
/*  0x468610 — locked wcsnlen wrapper                                  */
/* ================================================================== */
/*  Acquires/releases lock around wcsnlen call for thread safety.
 *  Address: 0x468610, size: 55 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x468650 — wcsnlen (full)                                          */
/* ================================================================== */
/*  Standard wcsnlen — wide string length with max bound.
 *  Address: 0x468650, size: 316 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x468790 — _fseek (locked)                                         */
/* ================================================================== */
/*  Acquires file lock, calls internal fseek (SetFilePointer), releases
 *  lock.
 *  Address: 0x468790, size: 50 bytes
 *  Calling convention: __cdecl
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x4687D0 — _fseek_nolock                                           */
/* ================================================================== */
/*  Internal fseek — handles SEEK_SET/CUR/END, flushes write buffer,
 *  calls SetFilePointer.
 *  Address: 0x4687D0, size: 151 bytes
 *  Calling convention: __cdecl
 *  src: _FILE.OBJ
 */

/* ================================================================== */
/*  0x468870 — _beginthreadex                                          */
/* ================================================================== */
/*  Creates a thread with CRT TLS support. Allocates per-thread data,
 *  calls CreateThread. Returns thread handle.
 *  Address: 0x468870, size: 123 bytes
 *  Calling convention: __cdecl
 *  src: _THREAD.OBJ
 */

/* ================================================================== */
/*  0x4688F0 — _threadstart                                            */
/* ================================================================== */
/*  Thread startup routine. Initializes TLS, calls user thread proc,
 *  then _endthreadex.
 *  Address: 0x4688F0, size: 140 bytes
 *  Calling convention: __cdecl
 *  src: _THREAD.OBJ
 */

/* ================================================================== */
/*  0x4689A0 — _endthreadex                                            */
/* ================================================================== */
/*  Thread cleanup. Calls thread detach callbacks, cleans up TLS,
 *  calls ExitThread.
 *  Address: 0x4689A0, size: 53 bytes
 *  Calling convention: __cdecl
 *  src: _THREAD.OBJ
 */

/* ================================================================== */
/*  0x4689E0 — _mainCRTStartup                                         */
/* ================================================================== */
/*  CRT entry point. Calls GetVersion, CRT init, GetCommandLineA,
 *  then jumps to real WinMain at 0x462E90.
 *  Address: 0x4689E0, size: 391 bytes
 *  Calling convention: __cdecl
 *  src: _CRT0.OBJ
 */

/* ================================================================== */
/*  0x468B90 — __amsg_exit                                             */
/* ================================================================== */
/*  Displays runtime error message and terminates.
 *  Address: 0x468B90, size: 38 bytes
 *  Calling convention: __cdecl
 *  src: _AMSG.OBJ
 */

/* ================================================================== */
/*  0x468BC0 — abort                                                   */
/* ================================================================== */
/*  Standard C abort — flushes stdout, calls ExitProcess(0xFF).
 *  Address: 0x468BC0, size: 38 bytes
 *  Calling convention: __cdecl
 *  src: _ABORT.OBJ
 */

/* ================================================================== */
/*  0x468BF0 — _close                                                  */
/* ================================================================== */
/*  Closes a file descriptor. Validates fd, locks file handle, calls
 *  CloseHandle.
 *  Address: 0x468BF0, size: 104 bytes
 *  Calling convention: __cdecl
 *  src: _CLOSE.OBJ
 */

/* ================================================================== */
/*  0x468C60 — _close_nolock                                           */
/* ================================================================== */
/*  Internal fd close. Gets OS handle via _get_osfhandle, calls
 *  CloseHandle, frees fd table entry.
 *  Address: 0x468C60, size: 144 bytes
 *  Calling convention: __cdecl
 *  src: _CLOSE.OBJ
 */

/* ================================================================== */
/*  0x468CF0 — _write                                                  */
/* ================================================================== */
/*  Writes data to a file descriptor. Validates fd, locks, calls
 *  WriteFile.
 *  Address: 0x468CF0, size: 114 bytes
 *  Calling convention: __cdecl
 *  src: _WRITE.OBJ
 */

/* ================================================================== */
/*  0x468D70 — wcstol                                                  */
/* ================================================================== */
/*  Standard wcstol — wide string to long.
 *  Address: 0x468D70, size: 521 bytes
 *  Calling convention: __cdecl
 */

/* ================================================================== */
/*  0x468F80 — _read                                                   */
